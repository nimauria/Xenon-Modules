#include "gracemeria/ac6_migration.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <regex>
#include <sstream>

namespace gracemeria::migration {

namespace {

namespace analysis = xenon::recomp::analysis;

std::string trim(const std::string& text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

std::string strip_comment(const std::string& line) {
  bool in_string = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"') in_string = !in_string;
    else if (line[i] == '#' && !in_string) return line.substr(0, i);
  }
  return line;
}

std::optional<std::uint32_t> parse_hex_or_dec(const std::string& text) {
  try {
    return static_cast<std::uint32_t>(std::stoul(text, nullptr, 0));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// A midasm_hook record accumulated across several `key = value` lines under
// one `[[midasm_hooks]]` table header (see ac6_migration.hpp's header
// comment for why this exact shape is inferred, not independently
// re-verified against the raw file).
struct PendingHook {
  std::optional<std::uint32_t> address;
  std::string name;
  std::vector<std::string> registers;
  std::optional<std::uint32_t> jump_address_on_true;
  bool any_field_seen{false};
};

std::string describe_hook(const PendingHook& hook) {
  std::ostringstream out;
  out << "midasm_hook";
  if (!hook.registers.empty()) {
    out << " registers=[";
    for (std::size_t i = 0; i < hook.registers.size(); ++i) {
      if (i) out << ",";
      out << hook.registers[i];
    }
    out << "]";
  }
  if (hook.jump_address_on_true) {
    std::ostringstream addr;
    addr << std::hex << *hook.jump_address_on_true;
    out << " jump_address_on_true=0x" << addr.str();
  }
  return out.str();
}

}  // namespace

std::string MigrationCategoryCounts::summary() const {
  std::ostringstream out;
  out << "functions_mapped=" << functions_mapped << " rexcrt_entries_seen=" << rexcrt_entries_seen
      << " native_replacements_mapped=" << native_replacements_mapped
      << " native_replacements_unsupported=" << native_replacements_unsupported
      << " indirect_calls_mapped=" << indirect_calls_mapped
      << " runtime_helpers_mapped=" << runtime_helpers_mapped
      << " midasm_hooks_mapped=" << midasm_hooks_mapped << " obsolete_fields=" << obsolete_fields.size()
      << " requires_investigation=" << requires_investigation.size();
  return out.str();
}

bool migrate_ac6_recomp_config(const std::string& toml_text, analysis::AnalysisHintSetV2& out,
                               MigrationCategoryCounts& counts, std::string& error) {
  out = analysis::AnalysisHintSetV2{};
  counts = MigrationCategoryCounts{};
  error.clear();

  static const std::regex kSectionHeader(R"(^\[([A-Za-z0-9_]+)\]$)");
  static const std::regex kArrayTableHeader(R"(^\[\[([A-Za-z0-9_]+)\]\]$)");
  // Custom raw-string delimiter ("re(...)re") for patterns whose content
  // contains a literal `)"` sequence (e.g. `[^"]*)"`), which would otherwise
  // prematurely terminate a plain R"(...)" raw string.
  static const std::regex kRootStringField(R"re(^([A-Za-z0-9_]+)\s*=\s*"([^"]*)"$)re");
  static const std::regex kRootAddressField(R"(^(longjmp_address|setjmp_address)\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)$)");
  static const std::regex kIndirectCalls(R"(^indirect_calls\s*=\s*\[(.*)\]$)");
  static const std::regex kRexcrtEntry(R"(^([A-Za-z0-9_]+)\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)$)");
  static const std::regex kFunctionEntry(
      R"re(^(0[xX][0-9A-Fa-f]+|\d+)\s*=\s*\{\s*name\s*=\s*"([^"]*)"\s*\}$)re");
  static const std::regex kHookAddress(R"(^address\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)$)");
  static const std::regex kHookName(R"re(^name\s*=\s*"([^"]*)"$)re");
  static const std::regex kHookRegisters(R"(^registers\s*=\s*\[(.*)\]$)");
  static const std::regex kHookJumpTrue(R"(^jump_address_on_true\s*=\s*(0[xX][0-9A-Fa-f]+|\d+|null)$)");

  std::string current_section;
  bool in_midasm_hooks_table = false;
  PendingHook pending_hook;

  const auto flush_pending_hook = [&]() {
    if (!pending_hook.any_field_seen) return;
    analysis::HookDeclaration hook{};
    hook.address = pending_hook.address.value_or(0);
    hook.native_replacement_identity = pending_hook.name;
    hook.description = describe_hook(pending_hook);
    out.hooks.push_back(std::move(hook));
    ++counts.midasm_hooks_mapped;
    pending_hook = PendingHook{};
  };

  std::istringstream stream(toml_text);
  std::string raw_line;
  std::size_t line_number = 0;
  while (std::getline(stream, raw_line)) {
    ++line_number;
    const auto line = trim(strip_comment(raw_line));
    if (line.empty()) continue;

    std::smatch match;
    if (std::regex_match(line, match, kArrayTableHeader)) {
      flush_pending_hook();
      const auto table_name = match[1].str();
      in_midasm_hooks_table = (table_name == "midasm_hooks" || table_name == "midasm_hook");
      current_section.clear();
      if (!in_midasm_hooks_table) {
        counts.requires_investigation.push_back("line " + std::to_string(line_number) +
                                                 ": unrecognized array-of-tables [[" + table_name + "]]");
      }
      continue;
    }
    if (std::regex_match(line, match, kSectionHeader)) {
      flush_pending_hook();
      in_midasm_hooks_table = false;
      current_section = match[1].str();
      continue;
    }

    if (in_midasm_hooks_table) {
      pending_hook.any_field_seen = true;
      if (std::regex_match(line, match, kHookAddress)) {
        pending_hook.address = parse_hex_or_dec(match[1].str());
        continue;
      }
      if (std::regex_match(line, match, kHookName)) {
        pending_hook.name = match[1].str();
        continue;
      }
      if (std::regex_match(line, match, kHookRegisters)) {
        std::istringstream list(match[1].str());
        std::string token;
        while (std::getline(list, token, ',')) {
          auto value = trim(token);
          if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
          }
          if (!value.empty()) pending_hook.registers.push_back(value);
        }
        continue;
      }
      if (std::regex_match(line, match, kHookJumpTrue)) {
        if (match[1].str() != "null") pending_hook.jump_address_on_true = parse_hex_or_dec(match[1].str());
        continue;
      }
      counts.requires_investigation.push_back("line " + std::to_string(line_number) +
                                              " (midasm_hooks entry): " + raw_line);
      continue;
    }

    if (current_section == "functions") {
      if (std::regex_match(line, match, kFunctionEntry)) {
        const auto address = parse_hex_or_dec(match[1].str());
        if (!address) {
          counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
          continue;
        }
        analysis::FunctionHint hint{};
        hint.address = *address;
        hint.name = match[2].str();
        out.functions.push_back(std::move(hint));
        ++counts.functions_mapped;
        continue;
      }
      counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
      continue;
    }

    if (current_section == "rexcrt") {
      if (std::regex_match(line, match, kRexcrtEntry)) {
        const auto address = parse_hex_or_dec(match[2].str());
        if (!address) {
          counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
          continue;
        }
        ++counts.rexcrt_entries_seen;
        analysis::NativeReplacement replacement{};
        replacement.guest_address = *address;
        replacement.source_name = match[1].str();
        replacement.kind = analysis::native_replacement_kind_from_name(replacement.source_name);
        if (replacement.kind == analysis::NativeReplacementKind::Unsupported) {
          counts.unsupported_native_replacement_names.push_back(replacement.source_name);
          ++counts.native_replacements_unsupported;
        } else {
          ++counts.native_replacements_mapped;
        }
        out.native_replacements.push_back(std::move(replacement));
        continue;
      }
      counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
      continue;
    }

    if (current_section.empty()) {
      if (std::regex_match(line, match, kRootAddressField)) {
        const auto address = parse_hex_or_dec(match[2].str());
        if (!address) {
          counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
          continue;
        }
        analysis::RuntimeHelper helper{};
        helper.address = *address;
        helper.kind = match[1].str() == "setjmp_address" ? analysis::RuntimeHelperKind::SetJmp
                                                          : analysis::RuntimeHelperKind::LongJmp;
        out.runtime_helpers.push_back(helper);
        ++counts.runtime_helpers_mapped;
        continue;
      }
      if (std::regex_match(line, match, kIndirectCalls)) {
        std::istringstream list(match[1].str());
        std::string token;
        while (std::getline(list, token, ',')) {
          const auto trimmed = trim(token);
          if (trimmed.empty()) continue;
          const auto address = parse_hex_or_dec(trimmed);
          if (!address) {
            counts.requires_investigation.push_back("line " + std::to_string(line_number) +
                                                     " (indirect_calls entry): " + trimmed);
            continue;
          }
          out.indirect_calls.push_back(analysis::KnownIndirectCall{*address, {}});
          ++counts.indirect_calls_mapped;
        }
        continue;
      }
      if (std::regex_match(line, match, kRootStringField)) {
        counts.obsolete_fields.push_back(match[1].str() + " = \"" + match[2].str() + "\"");
        continue;
      }
      counts.requires_investigation.push_back("line " + std::to_string(line_number) + ": " + raw_line);
      continue;
    }

    counts.requires_investigation.push_back("line " + std::to_string(line_number) + " (section [" +
                                            current_section + "]): " + raw_line);
  }
  flush_pending_hook();

  return true;
}

}  // namespace gracemeria::migration

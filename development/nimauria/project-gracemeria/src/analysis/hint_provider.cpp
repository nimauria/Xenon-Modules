#include "gracemeria/hint_provider.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "xenon/core/json.hpp"
#include "xenon/recomp/analysis_schema.hpp"
#include "xenon/recomp/analysis_schema_json.hpp"

#ifndef GRACEMERIA_REVISIONS_DIR
#error "GRACEMERIA_REVISIONS_DIR must be defined by the build (see CMakeLists.txt)"
#endif

namespace gracemeria {
namespace {
constexpr std::uint32_t kTitleId = 0x4E4D07D1u;
constexpr std::uint32_t kUsRetailMediaId = 0x531C30BEu;
// Europe Rev 1: addresses translated from the US profile by Xenon's
// hint-port (see revisions/eur-retail-rev1/revision.json).
constexpr std::uint32_t kEuRev1MediaId = 0x0379EFB3u;

std::string hex32(std::uint32_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << std::uppercase << value;
  return out.str();
}

bool read_json(const std::filesystem::path& path, xenon::core::JsonValue& json, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error = "Project Gracemeria: cannot open " + path.string();
    return false;
  }
  std::ostringstream buffer;
  buffer << file.rdbuf();
  if (!file.good() && !file.eof()) {
    error = "Project Gracemeria: failed reading " + path.string();
    return false;
  }
  std::string json_error;
  if (!xenon::core::JsonValue::parse(buffer.str(), json, &json_error)) {
    error = "Project Gracemeria: malformed " + path.string() + ": " + json_error;
    return false;
  }
  return true;
}

// Turns every hook hooks.json enables into an instruction hook named after
// it. The hint set's hooks are the migrated ReXGlue declarations and
// Gracemeria's own; hooks.json is Gracemeria's decision about which of them
// run, and how: "onTrue": "return" makes the routine return bool and, when
// it returns true, the hooked function returns to its caller (a hook at a
// function's entry that stands in for the whole function).
bool enable_classified_hooks(const std::filesystem::path& hooks_path,
                             xenon::recomp::analysis::AnalysisHintSetV2& hint_set,
                             std::string& error) {
  xenon::core::JsonValue json;
  if (!read_json(hooks_path, json, error)) return false;
  const auto* entries = json.find("hooks");
  if (!entries || !entries->as_array()) {
    error = "Project Gracemeria: " + hooks_path.string() + " has no hooks array";
    return false;
  }
  for (const auto& entry : *entries->as_array()) {
    if (!entry.get_bool("enabled")) continue;
    const auto address = entry.get("address").as_uint32();
    const auto name = entry.get_string("name");
    const auto on_true = entry.get_string("onTrue", "continue");
    if (on_true != "continue" && on_true != "return") {
      error = "Project Gracemeria: " + hooks_path.string() + " gives " + name + " onTrue \"" + on_true +
              "\" (expected \"continue\" or \"return\")";
      return false;
    }
    bool found = false;
    for (auto& hook : hint_set.hooks) {
      if (hook.address != address || hook.native_replacement_identity != name) continue;
      hook.kind = xenon::recomp::analysis::HookKind::Instruction;
      hook.symbol = name;
      hook.on_true = on_true == "return" ? xenon::recomp::analysis::HookAction::Return
                                         : xenon::recomp::analysis::HookAction::Continue;
      found = true;
    }
    if (!found) {
      error = "Project Gracemeria: " + hooks_path.string() + " enables " + name + " at " +
              hex32(address) + ", which the hint set does not declare";
      return false;
    }
  }
  return true;
}
}  // namespace

RevisionMatch match_supported_revision(const xenon::xbox::XexEffectiveIdentity& identity) {
  if (identity.title_id != kTitleId) {
    return {false, {}, "title ID " + hex32(identity.title_id) + " is not Ace Combat 6"};
  }
  if (identity.media_id != kUsRetailMediaId && identity.media_id != kEuRev1MediaId) {
    return {false, {}, "Ace Combat 6 media ID " + hex32(identity.media_id) +
                       " has no revision profile (US retail 0x531C30BE and Europe Rev 1 "
                       "0x0379EFB3 are supported)"};
  }
  // Each profile's fixed addresses are from that disc's own executable.
  // Xenon applies title updates to produce an effective XEX; once a TU's
  // version/hash is locally verified, it must receive a separate revision
  // profile rather than inheriting the base executable's addresses.
  if (identity.title_update_applied) {
    return {false, {}, "title-update effective image requires its own verified Gracemeria revision profile"};
  }
  // The returned id is the revision's on-disk directory name under
  // revisions/ (revision.json's "id" is longer descriptive metadata, not a
  // path) - provide() uses it directly to locate
  // <revisions_root>/<revision_id>/analysis/hints.v2.json.
  if (identity.media_id == kEuRev1MediaId) return {true, "eur-retail-rev1", "matched title/media identity"};
  return {true, "usa-retail-tu0", "matched title/media identity"};
}

Ac6HintProvider::Ac6HintProvider(std::filesystem::path revisions_root)
    : revisions_root_(std::move(revisions_root)) {}

std::filesystem::path Ac6HintProvider::default_revisions_root() {
  return std::filesystem::path(GRACEMERIA_REVISIONS_DIR);
}

bool Ac6HintProvider::provide(const xenon::xbox::XexEffectiveIdentity& identity,
                              xenon::recomp::analysis::AnalysisHintSetV2& out_hint_set,
                              std::string& error) const {
  const auto match = match_supported_revision(identity);
  if (!match.matched) {
    error = "Project Gracemeria: unsupported executable revision: " + match.reason;
    return false;
  }

  const auto hints_path = revisions_root_ / match.revision_id / "analysis" / "hints.v2.json";
  xenon::core::JsonValue json;
  if (!read_json(hints_path, json, error)) return false;

  xenon::recomp::analysis::AnalysisHintSetV2 loaded;
  std::string from_json_error;
  if (!xenon::recomp::analysis::from_json(json, loaded, from_json_error)) {
    error = "Project Gracemeria: invalid schema in " + hints_path.string() + ": " + from_json_error;
    return false;
  }

  if (!enable_classified_hooks(revisions_root_ / match.revision_id / "analysis" / "hooks.json",
                               loaded, error))
    return false;

  std::vector<std::string> schema_errors;
  if (!xenon::recomp::analysis::validate_hint_set(loaded, schema_errors)) {
    error = "Project Gracemeria: " + hints_path.string() + " failed hint-set validation:";
    for (const auto& schema_error : schema_errors) error += " " + schema_error + ";";
    return false;
  }

  // The on-disk file's identity is intentionally zeroed until a real
  // default.xex lets migrate-ac6-recomp scope it for real use (see
  // revisions/usa-retail-tu0/revision.json's notes). Stamp the EXACT
  // identity this provider was asked about instead (see hint_provider.hpp's
  // doc comment): eligibility was already decided above by title/media ID,
  // not by a fixed hash this revision does not have yet.
  loaded.module_name = match.revision_id;
  loaded.identity = identity;
  out_hint_set = std::move(loaded);
  return true;
}

bool Ac6HintProvider::hook_sources(const xenon::xbox::XexEffectiveIdentity& identity,
                                   std::vector<std::filesystem::path>& out_sources,
                                   std::string& error) const {
  const auto match = match_supported_revision(identity);
  if (!match.matched) {
    error = "Project Gracemeria: unsupported executable revision: " + match.reason;
    return false;
  }
  const auto directory = revisions_root_ / match.revision_id / "hooks";
  std::error_code ec;
  if (!std::filesystem::exists(directory, ec)) return true;
  std::vector<std::filesystem::path> found;
  for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file() && it->path().extension() == ".cpp") found.push_back(it->path());
  if (ec) {
    error = "Project Gracemeria: cannot read " + directory.string() + ": " + ec.message();
    return false;
  }
  std::sort(found.begin(), found.end());
  out_sources.insert(out_sources.end(), found.begin(), found.end());
  return true;
}

}  // namespace gracemeria

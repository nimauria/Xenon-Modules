// Development migration tool (title-specific, Gracemeria-owned - see
// gracemeria/ac6_migration.hpp): converts sal063/AC6_recomp's public
// ac6recomp_config.toml into Xenon's generic Analysis Hint Schema V2 JSON.
// Xenon's own runtime/Recomp Driver never reads ReXGlue TOML; this tool's
// only job is to produce a Gracemeria revision's analysis data file from it.

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "gracemeria/ac6_migration.hpp"
#include "xenon/recomp/analysis_schema_json.hpp"
#include "xenon/xbox/xex_loader.hpp"

namespace {

bool read_file(const std::string& path, std::string& out) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) return false;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  out = buffer.str();
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    std::cout << "migrate-ac6-recomp - AC6_recomp config -> Xenon Analysis Hint Schema V2\n"
                 "usage: migrate-ac6-recomp <ac6recomp_config.toml> [--out revisions/<id>/analysis/hints.v2.json]\n"
                 "                          [--identity-xex default.xex]\n"
                 "  --out          writes the converted hint set as JSON (defaults to stdout)\n"
                 "  --identity-xex scopes the output to a real title's effective executable\n"
                 "                 identity. Omitted: identity is left zeroed and MUST be filled\n"
                 "                 in (or the file re-generated with this flag) before the hint\n"
                 "                 set can pass Xenon's hint_set_matches_identity() check.\n";
    return argc < 2 ? 1 : 0;
  }

  std::string config_path = argv[1];
  std::string out_path;
  std::string identity_xex_path;
  for (int index = 2; index < argc; ++index) {
    const std::string arg = argv[index];
    if (arg == "--out" && index + 1 < argc) out_path = argv[++index];
    else if (arg == "--identity-xex" && index + 1 < argc) identity_xex_path = argv[++index];
    else {
      std::cerr << "migrate-ac6-recomp: unknown argument: " << arg << "\n";
      return 1;
    }
  }

  std::string toml_text;
  if (!read_file(config_path, toml_text)) {
    std::cerr << "migrate-ac6-recomp: unable to open " << config_path << "\n";
    return 2;
  }

  xenon::recomp::analysis::AnalysisHintSetV2 hint_set{};
  gracemeria::migration::MigrationCategoryCounts counts{};
  std::string error;
  if (!gracemeria::migration::migrate_ac6_recomp_config(toml_text, hint_set, counts, error)) {
    std::cerr << "migrate-ac6-recomp: " << error << "\n";
    return 3;
  }
  hint_set.module_name = "ProjectGracemeria";

  if (!identity_xex_path.empty()) {
    std::ifstream xex_file(identity_xex_path, std::ios::binary);
    if (!xex_file) {
      std::cerr << "migrate-ac6-recomp: unable to open --identity-xex " << identity_xex_path << "\n";
      return 2;
    }
    const std::vector<char> raw((std::istreambuf_iterator<char>(xex_file)), {});
    std::vector<std::byte> xex_bytes(raw.size());
    std::transform(raw.begin(), raw.end(), xex_bytes.begin(),
                   [](char value) { return static_cast<std::byte>(static_cast<unsigned char>(value)); });
    xenon::xbox::XexImage image{};
    std::string parse_error;
    if (!xenon::xbox::parse_xex_image(xex_bytes, image, &parse_error)) {
      std::cerr << "migrate-ac6-recomp: failed to parse --identity-xex: " << parse_error << "\n";
      return 2;
    }
    hint_set.identity = xenon::xbox::compute_effective_identity(image);
    std::cout << "# Scoped to effective executable revision "
              << xenon::xbox::format_effective_image_hash(hint_set.identity.effective_image_hash) << "\n";
  } else {
    std::cout << "# WARNING: no --identity-xex given; output identity is zeroed and is NOT yet\n"
                 "# usable by Xenon's ModuleHintProvider/Recomp Driver (hint_set_matches_identity()\n"
                 "# will never match a real executable). Re-run with --identity-xex once a\n"
                 "# legally supplied default.xex is available locally.\n";
  }

  std::vector<std::string> schema_errors;
  const bool valid = xenon::recomp::analysis::validate_hint_set(hint_set, schema_errors);

  std::cout << "# AC6 config migration report\n";
  std::cout << "# " << counts.summary() << "\n";
  std::cout << "# schema_valid=" << (valid ? "true" : "false") << "\n";
  for (const auto& schema_error : schema_errors) std::cout << "# schema-error: " << schema_error << "\n";
  if (!counts.obsolete_fields.empty()) {
    std::cout << "# obsolete/ReXGlue-tooling-only fields (not analysis metadata):\n";
    for (const auto& field : counts.obsolete_fields) std::cout << "#   " << field << "\n";
  }
  if (!counts.unsupported_native_replacement_names.empty()) {
    std::cout << "# rexcrt names with no verified Xenon implementation yet (recorded, not lost):\n";
    for (const auto& name : counts.unsupported_native_replacement_names) std::cout << "#   " << name << "\n";
  }
  if (!counts.requires_investigation.empty()) {
    std::cout << "# lines that matched no recognized category (reported, not dropped):\n";
    for (const auto& line : counts.requires_investigation) std::cout << "#   " << line << "\n";
  }

  const auto json_text = xenon::recomp::analysis::to_json(hint_set).dump(2);
  if (out_path.empty()) {
    std::cout << json_text << "\n";
  } else {
    std::ofstream output(out_path, std::ios::binary | std::ios::trunc);
    output << json_text;
    std::cout << "# wrote " << out_path << "\n";
  }

  return 0;
}

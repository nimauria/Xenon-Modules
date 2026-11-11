// gracemeria-export-analysis: writes Gracemeria's revision profiles as a
// Xenon module hint package, so the launcher's automatic preparation
// (xenon-prepare --module <module>/xenon-analysis) builds the native module
// for whichever disc the player has from this one module.
//
//   gracemeria-export-analysis <output-directory> [--revisions <revisions-root>]
//
// Layout written (xenon/recomp/module_hint_provider.hpp):
//   manifest.json                              moduleName, compatibilityVersion, revisions
//   revisions/<effective-image-hash>/analysis.json
//   revisions/<effective-image-hash>/hooks/*.cpp  (the revision's hook sources)
//   revisions/<effective-image-hash>/rendering.json  (from <revision>/rendering/, when present:
//     the temporal rendering profile Xenon reads at launch, xenon/gpu/temporal_profile.hpp)
//
// Each analysis.json is exactly what Ac6HintProvider supplies for that
// revision - hooks.json policy applied, identity stamped - so the launcher
// path and gracemeria-recomp compile the same hints.

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gracemeria/hint_provider.hpp"
#include "xenon/core/json.hpp"
#include "xenon/gpu/temporal_profile.hpp"
#include "xenon/recomp/analysis_schema_json.hpp"

namespace {

// Bump when a change can alter a prepared module without changing the
// exported analysis data or hook source text (see FileModuleHintProvider).
constexpr const char* kCompatibilityVersion = "1";

bool read_json(const std::filesystem::path& path, xenon::core::JsonValue& out, std::string& error) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  if (!file) {
    error = "cannot read " + path.string();
    return false;
  }
  if (!xenon::core::JsonValue::parse(text.str(), out, &error)) {
    error = path.string() + ": " + error;
    return false;
  }
  return true;
}

bool parse_hex(const std::string& text, std::uint64_t& out) {
  if (text.empty() || text.size() > 16) return false;
  out = 0;
  for (const char c : text) {
    const auto digit = std::isdigit(static_cast<unsigned char>(c)) ? c - '0'
                       : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                       : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                : -1;
    if (digit < 0) return false;
    out = out * 16u + static_cast<std::uint64_t>(digit);
  }
  return true;
}

bool parse_hash(const std::string& text, std::array<std::byte, 20>& out) {
  if (text.size() != 40) return false;
  for (std::size_t i = 0; i < 20; ++i) {
    std::uint64_t byte = 0;
    if (!parse_hex(text.substr(i * 2, 2), byte)) return false;
    out[i] = static_cast<std::byte>(byte);
  }
  return true;
}

bool write_text(const std::filesystem::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary);
  out << text;
  return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path output;
  std::filesystem::path revisions_root = GRACEMERIA_REVISIONS_DIR;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--revisions" && i + 1 < argc) revisions_root = argv[++i];
    else if (output.empty() && !arg.starts_with("--")) output = arg;
    else {
      std::cerr << "usage: gracemeria-export-analysis <output-directory> [--revisions <root>]\n";
      return 1;
    }
  }
  if (output.empty()) {
    std::cerr << "usage: gracemeria-export-analysis <output-directory> [--revisions <root>]\n";
    return 1;
  }

  std::vector<std::filesystem::path> revision_dirs;
  for (const auto& entry : std::filesystem::directory_iterator(revisions_root))
    if (entry.is_directory() && std::filesystem::exists(entry.path() / "revision.json"))
      revision_dirs.push_back(entry.path());
  std::sort(revision_dirs.begin(), revision_dirs.end());

  std::error_code ec;
  std::filesystem::remove_all(output, ec);
  std::filesystem::create_directories(output / "revisions");

  const gracemeria::Ac6HintProvider provider(revisions_root);
  auto revisions = xenon::core::JsonValue::make_array();
  for (const auto& dir : revision_dirs) {
    xenon::core::JsonValue revision;
    std::string error;
    if (!read_json(dir / "revision.json", revision, error)) {
      std::cerr << "gracemeria-export-analysis: " << error << "\n";
      return 2;
    }
    const auto identity_json = revision.get("identity");
    xenon::xbox::XexEffectiveIdentity identity{};
    std::uint64_t title = 0, media = 0;
    const auto media_value = identity_json.get("mediaIds");
    const auto* media_ids = media_value.as_array();
    const auto hash_text = identity_json.get_string("effectiveImageHash");
    if (!parse_hex(identity_json.get_string("titleId"), title) || !media_ids || media_ids->size() != 1 ||
        !parse_hex((*media_ids)[0].as_string(), media) || !parse_hash(hash_text, identity.effective_image_hash)) {
      std::cerr << "gracemeria-export-analysis: " << (dir / "revision.json").string()
                << " needs identity.titleId, one mediaId and a 40-digit effectiveImageHash\n";
      return 2;
    }
    identity.title_id = static_cast<std::uint32_t>(title);
    identity.media_id = static_cast<std::uint32_t>(media);

    xenon::recomp::analysis::AnalysisHintSetV2 hints;
    std::vector<std::filesystem::path> hook_sources;
    if (!provider.provide(identity, hints, error) || !provider.hook_sources(identity, hook_sources, error)) {
      std::cerr << "gracemeria-export-analysis: " << dir.filename().string() << ": " << error << "\n";
      return 2;
    }
    const auto target = output / "revisions" / hash_text;
    std::filesystem::create_directories(target);
    if (!write_text(target / "analysis.json", xenon::recomp::analysis::to_json(hints).dump(2) + "\n")) {
      std::cerr << "gracemeria-export-analysis: cannot write " << (target / "analysis.json").string() << "\n";
      return 2;
    }
    if (!hook_sources.empty()) std::filesystem::create_directories(target / "hooks");
    for (const auto& source : hook_sources)
      std::filesystem::copy_file(source, target / "hooks" / source.filename(),
                                 std::filesystem::copy_options::overwrite_existing);
    // The revision's rendering profile (temporal upscaling / frame
    // generation data), checked with Xenon's own parser so a bad one fails
    // here rather than at the player's launch.
    const auto rendering = dir / "rendering" / xenon::gpu::kRenderingProfileFile;
    const bool has_rendering = std::filesystem::exists(rendering);
    if (has_rendering) {
      std::ifstream in(rendering, std::ios::binary);
      std::ostringstream text;
      text << in.rdbuf();
      std::string rendering_error;
      (void)xenon::gpu::parse_temporal_render_profile(text.str(), &rendering_error);
      if (!in || !rendering_error.empty()) {
        std::cerr << "gracemeria-export-analysis: " << rendering.string() << ": "
                  << (rendering_error.empty() ? "cannot read" : rendering_error) << "\n";
        return 2;
      }
      std::filesystem::copy_file(rendering, target / xenon::gpu::kRenderingProfileFile,
                                 std::filesystem::copy_options::overwrite_existing);
    }
    revisions.append(xenon::core::JsonValue(hash_text));
    std::cout << dir.filename().string() << " -> revisions/" << hash_text << " ("
              << hints.functions.size() << " functions, " << hook_sources.size() << " hook sources"
              << (has_rendering ? ", rendering profile" : "") << ")\n";
  }

  auto manifest = xenon::core::JsonValue::make_object();
  manifest.set("moduleName", "Project Gracemeria");
  manifest.set("compatibilityVersion", kCompatibilityVersion);
  manifest.set("revisions", std::move(revisions));
  if (!write_text(output / "manifest.json", manifest.dump(2) + "\n")) {
    std::cerr << "gracemeria-export-analysis: cannot write manifest.json\n";
    return 2;
  }
  return 0;
}

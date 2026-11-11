#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "gracemeria/hint_provider.hpp"
#include "xenon/recomp/driver.hpp"
#include "xenon/recomp/game_intake.hpp"

namespace {
void usage() {
  std::cout << "usage: gracemeria-recomp <default.xex|disc image|game folder> [output-directory] [--json]\n"
               "  Title updates are Xenon runtime concerns (XenonSession::load_game() ->\n"
               "  xbox::apply_title_update()), not a Recomp Driver/static-analysis input -\n"
               "  the driver only ever analyzes the base executable given here. Gracemeria\n"
               "  deliberately does not accept a title update on this CLI (see\n"
               "  Ac6HintProvider - a title-update effective image needs its own verified\n"
               "  revision profile before Gracemeria supplies hints for it).\n";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    usage();
    return argc < 2 ? 1 : 0;
  }

  xenon::recomp::DriverOptions options;
  options.input = argv[1];
  options.output = "generated";
  // Byte-size shard budget instead of the driver's default fixed function
  // count (128/shard). Shard membership is purely a compile-unit grouping
  // decision - every cross-function call already goes through the runtime's
  // compiled-code lookup regardless of which shard the callee lives in, so
  // this has no effect on generated code or runtime behavior on any target,
  // handheld included. What it does affect is build time: bundling a fixed
  // *count* of functions per file produced wildly uneven shards (measured on
  // AC6: 358 KB to 38 MB, median 3.7 MB) because generated function size
  // varies by orders of magnitude - a handful of functions with large
  // jump-table-driven indirect-branch dispatches dominate a shard's size and
  // its /O2 compile time, dragging every other function bundled into that
  // same file along for the ride (observed: multi-hour single-shard
  // compiles). A byte budget bounds each shard's own compile cost directly;
  // a function larger than the budget still gets its own shard rather than
  // an oversized or empty one, isolating exactly the functions that were
  // causing the multi-hour outliers instead of fragmenting the entire
  // ~13,000-function corpus down to one file each.
  options.shard_max_bytes = 1024 * 1024;
  bool json = false;
  bool output_set = false;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
    } else if (!output_set && !arg.starts_with("--")) {
      options.output = arg;
      output_set = true;
    } else {
      std::cerr << "gracemeria-recomp: unknown argument: " << arg << '\n';
      usage();
      return 1;
    }
  }

  gracemeria::Ac6HintProvider provider;
  options.hint_provider_v2 = &provider;

  xenon::recomp::AnalysisReport report;
  std::string error;
  // A disc image or extracted folder is read the way Xenon's game
  // preparation reads it: its default.xex, without extracting to disk.
  if (std::filesystem::is_directory(options.input) || options.input.extension() != ".xex") {
    std::vector<std::byte> bytes;
    xenon::xbox::XexImage image;
    if (!xenon::recomp::read_game_executable_bytes(options.input, "default.xex", bytes, error) ||
        !xenon::xbox::parse_xex_image(bytes, image, &error)) {
      std::cerr << "gracemeria-recomp: " << options.input.string() << ": " << error << '\n';
      return 2;
    }
    options.pre_parsed_image = std::move(image);
  }
  if (!xenon::recomp::load_and_analyze(options, report, error)) {
    std::cerr << "gracemeria-recomp: " << error << '\n';
    return 2;
  }
  if (!xenon::recomp::generate_project(options, report, error)) {
    std::cerr << "gracemeria-recomp: " << error << '\n';
    return 3;
  }
  std::cout << (json ? xenon::recomp::format_report_json(report)
                     : xenon::recomp::format_report(report));
  return report.unresolved.empty() ? 0 : 4;
}

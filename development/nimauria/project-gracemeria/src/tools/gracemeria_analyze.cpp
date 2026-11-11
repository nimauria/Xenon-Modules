#include <iostream>
#include <string>

#include "gracemeria/hint_provider.hpp"
#include "xenon/recomp/driver.hpp"

// Runs only Xenon's static analysis phase (load_and_analyze) and always
// prints the resulting report as JSON, even when entry-integrity validation
// would later fail generate_project(). This is deliberately separate from
// gracemeria-recomp: analysis results (what Xenon's automatic discovery
// actually found, independent of the hints currently supplied) are exactly
// what tools/reduce_legacy_hints.py needs as input to compute which AC6_recomp
// legacy function starts are still missing from automatic discovery - and
// that comparison is most useful precisely when generation has not yet
// succeeded (see tools/reduce_legacy_hints.py's own docstring).
namespace {
void usage() {
  std::cout << "usage: gracemeria-analyze <default.xex>\n"
               "  Prints Xenon's static-analysis report (Analysis Hint Schema V2 - see\n"
               "  docs/recomp/ANALYSIS_HINT_SCHEMA_V2.md) as JSON, without running code\n"
               "  generation. Feed the output to tools/reduce_legacy_hints.py to find real\n"
               "  AC6_recomp legacy function starts Xenon's automatic discovery still misses.\n";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    usage();
    return argc < 2 ? 1 : 0;
  }

  xenon::recomp::DriverOptions options;
  options.input = argv[1];

  gracemeria::Ac6HintProvider provider;
  options.hint_provider_v2 = &provider;

  xenon::recomp::AnalysisReport report;
  std::string error;
  if (!xenon::recomp::load_and_analyze(options, report, error)) {
    std::cerr << "gracemeria-analyze: " << error << '\n';
    return 2;
  }
  std::cout << xenon::recomp::format_report_json(report);
  return 0;
}

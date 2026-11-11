// Tests for gracemeria::migration::migrate_ac6_recomp_config(). The fixture
// reproduces the confirmed real structure of sal063/AC6_recomp's public
// ac6recomp_config.toml (root project_name/file_path/out_directory_path
// strings, root longjmp_address/setjmp_address, a root indirect_calls
// array, a [rexcrt] section of name=address pairs, a [functions] section of
// address={name="..."} entries, and a [[midasm_hooks]] array-of-tables -
// see docs/AC6_RECOMP_AUDIT.md for the full real-file audit this mirrors),
// scaled down to a handful of entries rather than the real 10,527-function
// table, which is unnecessary to prove the migration logic and would only
// bloat this fixture. No copyrighted AC6 data is included.

#include "gracemeria/ac6_migration.hpp"
#include "xenon/recomp/analysis_schema_json.hpp"

#include <cassert>
#include <iostream>

using namespace xenon::recomp::analysis;
using namespace gracemeria::migration;

namespace {

constexpr const char* kSampleConfig = R"toml(
# ac6recomp - ReXGlue Codegen Configuration
project_name = "ac6recomp"
file_path = "assets/default.xex"
out_directory_path = "generated"

longjmp_address = 0x823CA690
setjmp_address = 0x823CA9B0

indirect_calls = [ 0x8237C368, 0x8237BA48 ]

[rexcrt]
RtlAllocateHeap = 0x821F9E10
memcpy = 0x82382F18
XMemCpy = 0x82382A90

[functions]
0x82090000 = { name = "rex_sub_82090000" }
0x820900C8 = { name = "rex_sub_820900C8" }

[[midasm_hooks]]
address = 0x8236D0E4
name = "ac6FlipIntervalHook"
registers = []
jump_address_on_true = 0x8236D0EC

[[midasm_hooks]]
address = 0x8236DE38
name = "ac6DeltaDivisorHook"
registers = ["r29"]
jump_address_on_true = null
)toml";

}  // namespace

int main() {
  std::cout << "Testing AC6 config migration (Gracemeria-owned)...\n";

  AnalysisHintSetV2 hint_set{};
  MigrationCategoryCounts counts{};
  std::string error;
  assert(migrate_ac6_recomp_config(kSampleConfig, hint_set, counts, error));
  assert(error.empty());

  assert(counts.functions_mapped == 2);
  assert(hint_set.functions.size() == 2);
  std::cout << "  [PASS] [functions] entries map to FunctionHint\n";

  assert(counts.runtime_helpers_mapped == 2);
  bool found_setjmp = false;
  for (const auto& helper : hint_set.runtime_helpers)
    if (helper.kind == RuntimeHelperKind::SetJmp) { found_setjmp = true; assert(helper.address == 0x823CA9B0u); }
  assert(found_setjmp);
  std::cout << "  [PASS] longjmp_address/setjmp_address map to RuntimeHelper\n";

  assert(counts.indirect_calls_mapped == 2);
  for (const auto& call : hint_set.indirect_calls) assert(call.targets.empty());
  std::cout << "  [PASS] indirect_calls maps to KnownIndirectCall (acknowledged, no fabricated targets)\n";

  assert(counts.rexcrt_entries_seen == 3);
  assert(counts.native_replacements_mapped == 2);   // RtlAllocateHeap, memcpy
  assert(counts.native_replacements_unsupported == 1);  // XMemCpy
  bool xmemcpy_reported = false;
  for (const auto& name : counts.unsupported_native_replacement_names)
    if (name == "XMemCpy") xmemcpy_reported = true;
  assert(xmemcpy_reported);
  std::cout << "  [PASS] [rexcrt] maps to NativeReplacement; unverified names are reported, not guessed\n";

  assert(counts.obsolete_fields.size() == 3);
  std::cout << "  [PASS] project_name/file_path/out_directory_path are reported as obsolete, not lost\n";

  // midasm_hooks: mapped to HookDeclaration, register/jump-target detail
  // preserved in the description (Xenon's generic schema has no dedicated
  // fields for those - nothing is silently dropped).
  assert(counts.midasm_hooks_mapped == 2);
  assert(hint_set.hooks.size() == 2);
  bool found_flip_hook = false;
  for (const auto& hook : hint_set.hooks) {
    if (hook.native_replacement_identity == "ac6FlipIntervalHook") {
      found_flip_hook = true;
      assert(hook.address == 0x8236D0E4u);
      assert(hook.description.find("8236d0ec") != std::string::npos);
    }
  }
  assert(found_flip_hook);
  std::cout << "  [PASS] [[midasm_hooks]] maps to HookDeclaration with register/jump detail preserved\n";

  assert(counts.requires_investigation.empty());
  std::cout << "  [PASS] A fully-recognized config produces zero requires_investigation entries\n";

  hint_set.identity.title_id = 0x4E4D07D1u;
  std::vector<std::string> schema_errors;
  assert(validate_hint_set(hint_set, schema_errors) && schema_errors.empty());
  std::cout << "  [PASS] Migrated hint set passes schema validation once given an identity\n";

  // An unrecognized section/line is reported, never silently lost.
  {
    const std::string config_with_unknown = std::string(kSampleConfig) +
        "\n[some_future_section]\nsome_future_key = 0x1234\n"
        "an_unparseable_root_line ??? not a key-value pair at all\n";
    AnalysisHintSetV2 hint_set2{};
    MigrationCategoryCounts counts2{};
    std::string error2;
    assert(migrate_ac6_recomp_config(config_with_unknown, hint_set2, counts2, error2));
    bool found_unknown_section = false, found_unparseable = false;
    for (const auto& line : counts2.requires_investigation) {
      if (line.find("some_future_section") != std::string::npos) found_unknown_section = true;
      if (line.find("an_unparseable_root_line") != std::string::npos) found_unparseable = true;
    }
    assert(found_unknown_section && found_unparseable);
  }
  std::cout << "  [PASS] An unrecognized section/line is reported in requires_investigation\n";

  // JSON round trip.
  {
    const auto json_text = to_json(hint_set).dump();
    xenon::core::JsonValue reparsed;
    std::string parse_error;
    assert(xenon::core::JsonValue::parse(json_text, reparsed, &parse_error));
    AnalysisHintSetV2 restored{};
    std::string from_json_error;
    assert(from_json(reparsed, restored, from_json_error));
    assert(restored.functions.size() == hint_set.functions.size());
    assert(restored.hooks.size() == hint_set.hooks.size());
  }
  std::cout << "  [PASS] Migrated hint set round-trips through JSON\n";

  std::cout << "All AC6 migration tests passed!\n";
  return 0;
}

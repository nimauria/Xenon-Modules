#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gracemeria/hint_provider.hpp"

int main() {
  gracemeria::Ac6HintProvider provider;
  xenon::recomp::analysis::AnalysisHintSetV2 hint_set;
  std::string error;

  xenon::xbox::XexEffectiveIdentity supported{};
  supported.title_id = 0x4E4D07D1u;
  supported.media_id = 0x531C30BEu;
  assert(provider.provide(supported, hint_set, error));
  assert(error.empty());
  // These come from the real, on-disk revisions/usa-retail-tu0/analysis/
  // hints.v2.json - loaded, not hand-duplicated in this provider (see
  // hint_provider.hpp/.cpp) - so this also proves the provider stays in
  // sync with whatever migrate-ac6-recomp actually produced, including the
  // 34 midasm hooks a smaller hand-maintained subset previously dropped.
  assert(hint_set.native_replacements.size() == 16);
  // setjmp, longjmp and the two register save/restore ladders.
  assert(hint_set.runtime_helpers.size() == 4);
  assert(hint_set.indirect_calls.size() == 2);
  // The 34 migrated midasm hooks plus Gracemeria's four host-timing hooks.
  assert(hint_set.hooks.size() == 38);
  // hooks.json enables the twelve storage-order hooks and the four
  // host-timing hooks, each as an instruction hook named after itself; the
  // rest stay metadata. The effect dispatcher and both cutscene Exec hooks
  // stand in for their whole function, so they return to the caller.
  std::size_t instruction_hooks = 0, storage_hooks = 0, returning_hooks = 0;
  for (const auto& hook : hint_set.hooks) {
    if (hook.kind != xenon::recomp::analysis::HookKind::Instruction) continue;
    ++instruction_hooks;
    assert(hook.symbol == hook.native_replacement_identity);
    assert(!hook.after_instruction);
    assert(hook.on_false == xenon::recomp::analysis::HookAction::Continue);
    if (hook.symbol.rfind("ac6StorageOrderHook", 0) == 0) {
      ++storage_hooks;
      assert(hook.on_true == xenon::recomp::analysis::HookAction::Continue);
    } else if (hook.symbol == "ac6EffectModeSubmitHook") {
      assert(hook.address == 0x821D4A78u);
      assert(hook.on_true == xenon::recomp::analysis::HookAction::Continue);
    } else {
      ++returning_hooks;
      assert(hook.on_true == xenon::recomp::analysis::HookAction::Return);
      assert((hook.symbol == "ac6EffectModeDispatchHook" && hook.address == 0x822B3248u) ||
             (hook.symbol == "ac6CutsceneResyncHookDD" && hook.address == 0x82184460u) ||
             (hook.symbol == "ac6CutsceneResyncHookEM" && hook.address == 0x821856F8u));
    }
  }
  assert(instruction_hooks == 16 && storage_hooks == 12 && returning_hooks == 3);
  {
    std::vector<std::filesystem::path> sources;
    std::string sources_error;
    assert(provider.hook_sources(supported, sources, sources_error) && sources_error.empty());
    std::vector<std::string> names;
    for (const auto& source : sources) names.push_back(source.filename().string());
    assert((names == std::vector<std::string>{"ac6_cutscene_resync_hooks.cpp", "ac6_effect_mode_hooks.cpp",
                                              "ac6_guest_call.cpp", "ac6_storage_order_hooks.cpp"}));
  }
  // The provider must stamp the hint set with the EXACT identity it was
  // asked about (see hint_provider.hpp), so Xenon's own
  // hint_set_matches_identity() safety check passes.
  assert(xenon::recomp::analysis::hint_set_matches_identity(hint_set, supported));
  std::vector<std::string> schema_errors;
  assert(xenon::recomp::analysis::validate_hint_set(hint_set, schema_errors) && schema_errors.empty());

  // Europe Rev 1: the same hints at its own addresses. It already contains
  // the title update's storage-order fix, so no hook runs and no hook source
  // is compiled.
  {
    auto europe = supported;
    europe.media_id = 0x0379EFB3u;
    xenon::recomp::analysis::AnalysisHintSetV2 eu_hints;
    std::string eu_error;
    assert(gracemeria::match_supported_revision(europe).revision_id == "eur-retail-rev1");
    assert(provider.provide(europe, eu_hints, eu_error) && eu_error.empty());
    assert(eu_hints.functions.size() == 372);
    assert(eu_hints.native_replacements.size() == 16);
    assert(eu_hints.indirect_calls.size() == 2);
    for (const auto& hook : eu_hints.hooks)
      assert(hook.kind == xenon::recomp::analysis::HookKind::Metadata);
    assert(xenon::recomp::analysis::hint_set_matches_identity(eu_hints, europe));
    // Translated, not copied: the US setjmp helper sits elsewhere in EU.
    bool moved = false;
    for (std::size_t i = 0; i < eu_hints.runtime_helpers.size(); ++i)
      moved |= eu_hints.runtime_helpers[i].address != hint_set.runtime_helpers[i].address;
    assert(moved);
    std::vector<std::filesystem::path> sources;
    assert(provider.hook_sources(europe, sources, eu_error) && sources.empty());
  }

  hint_set = {};
  error.clear();
  auto wrong_media = supported;
  wrong_media.media_id = 0x36D871BCu;
  assert(!provider.provide(wrong_media, hint_set, error));
  assert(!error.empty());

  hint_set = {};
  error.clear();
  auto wrong_title = supported;
  wrong_title.title_id = 0x12345678u;
  assert(!provider.provide(wrong_title, hint_set, error));
  assert(!error.empty());

  hint_set = {};
  error.clear();
  auto effective_tu = supported;
  effective_tu.title_update_applied = true;
  assert(!provider.provide(effective_tu, hint_set, error));
  assert(!error.empty());

  // A revisions root with no matching revision directory is a hard failure,
  // never a silent empty/fallback hint set (no-stub rule).
  {
    const auto temp_root = std::filesystem::temp_directory_path() /
                            "gracemeria_hint_provider_tests_empty_root";
    std::filesystem::create_directories(temp_root);
    gracemeria::Ac6HintProvider missing_data_provider(temp_root);
    xenon::recomp::analysis::AnalysisHintSetV2 empty_hint_set;
    std::string missing_error;
    assert(!missing_data_provider.provide(supported, empty_hint_set, missing_error));
    assert(!missing_error.empty());
    std::filesystem::remove_all(temp_root);
  }

  // hooks.json enabling a hook the hint set does not declare is a hard
  // failure, not a hook that silently never runs.
  {
    const auto temp_root = std::filesystem::temp_directory_path() /
                            "gracemeria_hint_provider_tests_unknown_hook";
    const auto analysis_dir = temp_root / "usa-retail-tu0" / "analysis";
    std::filesystem::create_directories(analysis_dir);
    std::filesystem::copy_file(std::filesystem::path(GRACEMERIA_REVISIONS_DIR) / "usa-retail-tu0" /
                                   "analysis" / "hints.v2.json",
                               analysis_dir / "hints.v2.json",
                               std::filesystem::copy_options::overwrite_existing);
    {
      std::ofstream out(analysis_dir / "hooks.json");
      out << R"({"hooks":[{"address":4096,"name":"notDeclaredHook","enabled":true}]})";
    }
    gracemeria::Ac6HintProvider unknown_hook_provider(temp_root);
    xenon::recomp::analysis::AnalysisHintSetV2 unknown_hint_set;
    std::string unknown_error;
    assert(!unknown_hook_provider.provide(supported, unknown_hint_set, unknown_error));
    assert(unknown_error.find("notDeclaredHook") != std::string::npos);

    // An action hooks.json cannot express is refused, not run as a plain
    // void hook.
    {
      std::ofstream out(analysis_dir / "hooks.json");
      out << R"({"hooks":[{"address":2182954124,"name":"ac6StorageOrderHookOp2","enabled":true,)"
             R"("onTrue":"jump"}]})";
    }
    unknown_hint_set = {};
    unknown_error.clear();
    assert(!unknown_hook_provider.provide(supported, unknown_hint_set, unknown_error));
    assert(unknown_error.find("onTrue") != std::string::npos);
    std::filesystem::remove_all(temp_root);
  }

  // A structurally malformed hints.v2.json is reported, never partially
  // applied.
  {
    const auto temp_root = std::filesystem::temp_directory_path() /
                            "gracemeria_hint_provider_tests_malformed_root";
    const auto analysis_dir = temp_root / "ac6-us-retail-531c30be-tu0" / "analysis";
    std::filesystem::create_directories(analysis_dir);
    {
      std::ofstream out(analysis_dir / "hints.v2.json");
      out << "{ not valid json";
    }
    gracemeria::Ac6HintProvider malformed_provider(temp_root);
    xenon::recomp::analysis::AnalysisHintSetV2 malformed_hint_set;
    std::string malformed_error;
    assert(!malformed_provider.provide(supported, malformed_hint_set, malformed_error));
    assert(!malformed_error.empty());
    std::filesystem::remove_all(temp_root);
  }

  return 0;
}

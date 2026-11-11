#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "xenon/recomp/driver.hpp"

namespace gracemeria {

struct RevisionMatch {
  bool matched{};
  std::string revision_id;
  std::string reason;
};

// Eligibility check independent of any exact effective-image hash (Gracemeria
// does not yet have one locked in for this revision - see revisions/
// usa-retail-tu0/revision.json's "effectiveXexSha1: null" and its notes).
// Matches on title ID + media ID + "not a title-update-patched image" only.
[[nodiscard]] RevisionMatch match_supported_revision(const xenon::xbox::XexEffectiveIdentity& identity);

// Production xenon::recomp::ModuleHintProviderV2 for Ace Combat 6 (see
// Xenon's docs/ANALYSIS_HINT_SCHEMA_V2.md for the schema/interface this
// implements). Supplies the Analysis Hint Schema V2 hint set for each
// address-verified revision profile: US retail (Media ID 0x531C30BE, base
// executable / no title update, migrated from AC6_recomp) and Europe Rev 1
// (Media ID 0x0379EFB3, translated from the US profile by Xenon's
// hint-port).
//
// This provider does not yet know a fixed effective-image SHA-1 to compare
// against (that requires a legally supplied default.xex to hash locally),
// so its eligibility test (match_supported_revision) is title/media-ID-based
// rather than hash-based. Once eligible, it stamps the returned hint set
// with the EXACT identity it was asked about (never a different/assumed
// one), so Xenon's own hint_set_matches_identity() safety check - which
// still runs after this provider returns - trivially and correctly passes;
// this provider's own matching policy is what actually decides eligibility,
// not a bypass of that check.
//
// Loads its hint data from `<revisions_root>/<revision_id>/analysis/
// hints.v2.json` (xenon::recomp::analysis::from_json) rather than duplicating
// it as hand-maintained C++ literals - that file is the same
// migrate-ac6-recomp output docs/AC6_RECOMP_AUDIT.md describes (all 34
// midasm hooks included), so there is exactly one place this data can drift.
// A missing/malformed/schema-invalid file is a hard provide() failure, never
// a silent fallback to a smaller built-in subset.
//
// Which hooks run is decided by `analysis/hooks.json`: each entry marked
// "enabled" becomes a Xenon instruction hook (HookKind::Instruction) whose
// routine has the hook's name, implemented in `<revision>/hooks/*.cpp`
// (hook_sources()). An entry's "onTrue": "return" makes the routine return
// bool and the hooked function return to its caller when it returns true. An
// enabled entry the hint set does not declare is a hard failure.
class Ac6HintProvider final : public xenon::recomp::ModuleHintProviderV2 {
 public:
  // `revisions_root` defaults to this source tree's own revisions/
  // directory (set at configure time via the GRACEMERIA_REVISIONS_DIR
  // compile definition - see CMakeLists.txt), which is what gracemeria-recomp
  // and the unit tests use. Packaging Gracemeria as an installed module later
  // moves this default to the installed package's own revisions/ directory;
  // callers that already know a specific root (tests exercising a malformed
  // or missing file) pass it explicitly instead.
  explicit Ac6HintProvider(std::filesystem::path revisions_root = default_revisions_root());

  [[nodiscard]] bool provide(const xenon::xbox::XexEffectiveIdentity& identity,
                             xenon::recomp::analysis::AnalysisHintSetV2& out_hint_set,
                             std::string& error) const override;
  [[nodiscard]] bool hook_sources(const xenon::xbox::XexEffectiveIdentity& identity,
                                  std::vector<std::filesystem::path>& out_sources,
                                  std::string& error) const override;

 private:
  [[nodiscard]] static std::filesystem::path default_revisions_root();

  std::filesystem::path revisions_root_;
};

}  // namespace gracemeria

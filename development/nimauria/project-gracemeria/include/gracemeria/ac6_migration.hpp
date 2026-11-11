#pragma once

// AC6 config migration (title-specific development tooling - owned by
// Project Gracemeria, not Xenon). Converts sal063/AC6_recomp's
// ac6recomp_config.toml (a ReXGlue codegen configuration - research
// reference per Xenon's CLAUDE.md, independently reimplemented here, not
// copied) into Xenon's generic Analysis Hint Schema V2
// (xenon::recomp::analysis::AnalysisHintSetV2). Xenon owns the schema/
// validation/driver-consumption; Gracemeria owns the AC6-specific knowledge
// of how to produce one. The dependency direction is Gracemeria -> Xenon,
// never the reverse - Xenon has no AC6-specific parser, namespace, address,
// or configuration format anywhere in its own tree.
//
// Confirmed present in the full, real ac6recomp_config.toml (per
// docs/AC6_RECOMP_AUDIT.md's complete audit of the supplied file - 10,527
// `functions` records, 2 `indirect_calls` sites, 16 `rexcrt` mappings,
// setjmp_address/longjmp_address, and 34 `midasm_hook` records; no
// switch-table, function-chunk, or explicit invalid/data-region
// declarations):
//
//   project_name = "..."             (root string - obsolete/tooling-only)
//   file_path = "..."                (root string - obsolete/tooling-only)
//   out_directory_path = "..."       (root string - obsolete/tooling-only)
//   longjmp_address = 0x...          (root hex int -> RuntimeHelper)
//   setjmp_address = 0x...           (root hex int -> RuntimeHelper)
//   indirect_calls = [0x.., 0x..]    (root hex array -> KnownIndirectCall, empty targets)
//   [rexcrt]
//   <name> = 0x...                   (-> NativeReplacement)
//   [functions]
//   0x... = { name = "..." }         (-> FunctionHint)
//   [[midasm_hooks]]                 (-> HookDeclaration; TOML key spelling
//   address = 0x...                  inferred from docs/AC6_RECOMP_AUDIT.md's
//   name = "..."                     already-completed classification in
//   registers = ["r29", ...]         revisions/usa-retail-tu0/analysis/hooks.json
//   jump_address_on_true = 0x...     - NOT independently re-verified against
//                                    the raw file byte-for-byte; see the .cpp
//                                    for what happens if the real spelling
//                                    differs (nothing is silently lost either
//                                    way - see MigrationCategoryCounts below).
//
// Any line this parser does not recognize is preserved verbatim in
// MigrationCategoryCounts::requires_investigation rather than silently
// dropped - a genuinely unsupported AC6 analysis category must be visible,
// never lost.

#include <cstddef>
#include <string>
#include <vector>

#include "xenon/recomp/analysis_schema.hpp"

namespace gracemeria::migration {

struct MigrationCategoryCounts {
  std::size_t functions_mapped{};
  std::size_t rexcrt_entries_seen{};
  std::size_t native_replacements_mapped{};
  std::size_t native_replacements_unsupported{};
  std::size_t indirect_calls_mapped{};
  std::size_t runtime_helpers_mapped{};
  std::size_t midasm_hooks_mapped{};
  // Root string fields (project_name/file_path/out_directory_path) that are
  // real ReXGlue tooling configuration, not Xenon analysis metadata -
  // reported explicitly, never silently discarded.
  std::vector<std::string> obsolete_fields;
  // rexcrt names this build recognizes syntactically but Xenon has no
  // verified native-replacement implementation for (see Xenon's
  // native_replacements.hpp) - still recorded as a NativeReplacement with
  // kind Unsupported, never dropped.
  std::vector<std::string> unsupported_native_replacement_names;
  // Any input line that matched none of the recognized shapes above.
  std::vector<std::string> requires_investigation;

  [[nodiscard]] std::string summary() const;
};

// Parses `toml_text` and fills `out`/`counts`. `out.identity` is left
// default-constructed (all zero) - the config itself carries no executable
// identity, so the caller must fill it in from a real title's XEX (see
// xenon::xbox::compute_effective_identity()) before this hint set is usable
// by anything that checks xenon::recomp::analysis::hint_set_matches_identity()
// - this function does not do that itself since a dry run may have no XEX at
// all. Returns false only on a structurally unreadable input; unrecognized
// *content* is reported via `counts`, not a parse failure.
[[nodiscard]] bool migrate_ac6_recomp_config(const std::string& toml_text,
                                             xenon::recomp::analysis::AnalysisHintSetV2& out,
                                             MigrationCategoryCounts& counts, std::string& error);

}  // namespace gracemeria::migration

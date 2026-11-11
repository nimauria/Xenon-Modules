# AC6_recomp migration audit

The supplied AC6_recomp tree was treated as a behavioural/research reference, not as the target architecture.

## Configuration migration

The complete supplied `ac6recomp_config.toml` was parsed. Its actual contents are narrower than the full set of concepts ReXGlue can represent:

- 10,527 `functions` records; 10,492 contain only a generated `rex_sub_*` name, 35 are empty.
- 2 `indirect_calls` sites.
- 16 `rexcrt` address mappings.
- `setjmp_address` and `longjmp_address`.
- 34 `midasm_hook` records.
- No switch-table declarations in this AC6 config.
- No function parent/chunk relationships in this AC6 config.
- No explicit invalid-code/data/exclusion regions in this AC6 config.
- No explicit indirect-call target sets in this AC6 config.

The 10k+ function list is retained as reference data, not blindly injected as Xenon boundaries. Automatic Xenon analysis gets first priority; the reduction tool emits only missed starts once real game content is available.

### Generic-schema migration tool

`gracemeria::migration::migrate_ac6_recomp_config()` (`include/gracemeria/ac6_migration.hpp`,
`src/analysis/ac6_migration.cpp`, CLI: `migrate-ac6-recomp`) converts
`ac6recomp_config.toml` directly into Xenon's generic
`xenon::recomp::analysis::AnalysisHintSetV2` - the schema Xenon's Recomp
Driver actually consumes (`ModuleHintProviderV2`), replacing the earlier,
now-removed dependency on an aspirational `ModuleHint.indirect_call_sites`
field that never existed in Xenon. `revisions/usa-retail-tu0/analysis/hints.v2.json`
is this tool's output, run against the already-migrated real address/name
data recorded in `hints.json`/`hooks.json`/`native-replacements.json`
(functions/no `[functions]` reconstructed - the 10k+ table remains reference-
only per the paragraph above): 16 rexcrt entries (15 mapped, `XMemCpy`
reported as unsupported/unverified), 2 indirect-call sites, setjmp/longjmp,
and all 34 midasm hooks, with zero entries landing in
`requires_investigation`. Its identity is intentionally left zeroed - see
`revision.json`'s notes - until a real `default.xex` lets `migrate-ac6-recomp
--identity-xex` (or a from-scratch re-run against the real, complete
`ac6recomp_config.toml`) scope it for real use. `Ac6HintProvider`
(`gracemeria::Ac6HintProvider`, the real `ModuleHintProviderV2` Xenon's Recomp
Driver calls) now loads `hints.v2.json` from disk (stamping the loaded set's
`identity`/`module_name` with the exact identity it was asked about, per its
own eligibility policy) instead of duplicating a hand-maintained subset in
code - the prior in-code version only carried native replacements/runtime
helpers/indirect calls and silently omitted all 34 migrated hooks from what
the driver actually received. Fixing this surfaced and fixed a real,
previously-latent bug: `match_supported_revision()` returned the descriptive
slug `ac6-us-retail-531c30be-tu0` as its revision id, which never matched the
actual on-disk directory name `usa-retail-tu0` - harmless while nothing read
from disk, but it would have made every `provide()` call fail once disk
loading was added, had it not been caught by `gracemeria_hint_provider_tests`
(see that test file's assertion on `hint_set.hooks.size() == 34`, and the two
new cases covering a missing-revision-directory and a malformed
`hints.v2.json`).

## Source classification

| AC6_recomp area | Classification | Migration result |
|---|---|---|
| `ac6recomp_config.toml` | AC6 analysis knowledge | Translated into revision-scoped JSON/CSV and ModuleHint data |
| `generated/` PPC->C++ | Generated reference | Not copied; Xenon Recomp Driver regenerates native code |
| `thirdparty/rexglue-sdk/` | ReXGlue/Xenia runtime plumbing | Discarded |
| `src/d3d_hooks.*` | ReXGlue/Xenia graphics plumbing + research | Runtime code discarded; observations retained conceptually |
| `src/render_hooks.*` | ReXGlue timing/present hooks | Not copied; enhancement research classified |
| `ac6_native_graphics*` | Backend diagnostics/experimental renderer | Discarded from production module |
| `ac6_backend_capture_bridge*` | Backend diagnostics | Research-only |
| `ac6_backend_pass_classifier*` | Backend diagnostics/classification | Research-only |
| `ac6_cutscene_resync*` | AC6 optional compatibility enhancement | Preserved as enhancement research, not ReXGlue code |
| `ac6_effect_mode_fix*` | Scheduler/backend-specific compatibility fix | Requires Xenon real-title validation; no blind port |
| `ac6_fps_physics_fix*` | AC6 60 FPS enhancement | Preserved as enhancement research |
| `ac6_fullres_effects*` | Renderer-specific enhancement/fix | Not copied into Xenon GPU path |
| `ac6_kbm_input*` | ReXGlue input replacement | Discarded; Xenon owns input |
| `ac6_storage_submit_order_fix*` | Base-game race workaround matching vendor TU | Not enabled; official TU path preferred |
| `ac6_widescreen*` | Optional AC6 enhancement tightly coupled to backend | Preserved as research only |
| PAC decode/probe/index sources | Reverse-engineering tooling | Research-only; no production hooks migrated |
| texture override/native asset tooling | Modding/host feature | Out of baseline Xenon module scope |
| build/bootstrap scripts | ReXGlue-specific build plumbing | Rewritten around current Xenon |

## Hook classification

Every one of the 34 config hook records is preserved in `revisions/usa-retail-tu0/analysis/hooks.json` with an explicit classification and is disabled by default. This prevents a ReXGlue workaround from becoming a hidden Xenon correctness patch.

## Generic behavior

Heap/CRT helpers such as `memcpy`, `memset`, `strlen`-family equivalents and RTL heap functions remain generic. Gracemeria records guest address/name pairs as candidates but does not ship duplicate host implementations.

## See also

- `../../MULTI_TITLE_MODULE_FINDINGS.md` - cross-title comparison against Project Gaia (Sonic Unleashed), written after Project Gaia's bootstrap.
- `../../GENERIC_XENON_BLOCKERS.md` - generic Xenon Analysis Hint Schema V2 gaps found while bootstrapping Project Gaia (none apply to this AC6 migration directly, but the module-ABI/hint-loading lessons in the "duplicated module helpers" section do).

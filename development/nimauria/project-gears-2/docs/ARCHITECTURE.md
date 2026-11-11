# Architecture

Project Gears 2 follows the same split as Project Gracemeria (Ace Combat 6):

1. **Xenon** reads `default.xex` from the player's disc, decrypts it, analyses it and translates every function to
   native C++, then builds the game module (a DLL) through `xenon-prepare`. Xenon provides the whole Xbox 360
   runtime.
2. **This module** contributes data keyed by the executable's identity:
   - the effective image hash selects `xenon-analysis/revisions/<hash>/analysis.json`, so hints never apply to a
     different executable (a title update, another region with different code);
   - hints correct analysis where automatic discovery is wrong (function bounds, indirect call targets);
   - instruction hooks run game-specific C++ at a guest address for genuine game-logic fixes.
3. **Authoring** happens in `revisions/<revision>/`. `tools/export_xenon_analysis.py` writes the package;
   `tools/validate_metadata.py` and the `--check` export are run by `ctest`.

## Adding a hint or hook

1. Record what showed the need: the analysis failure, trace or COGBox note, in the entry's `source`.
2. Add it to `analysis/hints.json` or `analysis/hooks.json` (plus `hooks/<name>.cpp`).
3. Add a hook test in `tests/<name>_hook_tests.cpp` for every hook.
4. Run the exporter, the validator and `ctest`; then re-run game preparation (`--force` after hook-only edits,
   since the artifact key does not include hook source text).

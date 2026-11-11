# Project Gracemeria

Project Gracemeria is the **Ace Combat 6: Fires of Liberation** game module for **Project Xenon**.

It is not an Xbox 360 runtime and does not contain a second CPU, memory system, kernel, XAM implementation, filesystem, audio stack, input stack, content system or Xenos renderer. Those belong to Xenon. Gracemeria supplies only title-specific information needed to identify, analyse and recompile Ace Combat 6, plus AC6-specific compatibility knowledge that remains valid on Xenon's runtime.

## Architecture

```text
user-owned Ace Combat 6 content
        |
        v
Project Gracemeria
  - revision identity
  - analysis hints / symbols
  - AC6 compatibility metadata
  - optional enhancement research
        |
        v
Project Xenon
  XEX Loader V2 -> Recomp Driver / CPU V2 -> xenon_game_module
  Memory V2 / Kernel / XAM / Filesystem / Content / Input / Audio / Xenos GPU
        |
        v
XenonRuntime / XenonSession
```

The generated native library uses Xenon's canonical `Xenon_BindCompiledRegistry` ABI. Gracemeria does not invent a title-specific loader.

## Current source status

The old Project Gracemeria subsystem skeleton has been removed. The current repository is a Xenon module/tooling repository.

Each supported disc has its own revision profile, keyed by its effective executable hash:

| Profile | Disc | Media ID | Effective image hash |
| --- | --- | --- | --- |
| `revisions/usa-retail-tu0/` | USA, base executable | `531C30BE` | `464905ef5d5a80d8b52697f72f16e8c93c4b81ba` |
| `revisions/eur-retail-rev1/` | Europe Rev 1 | `0379EFB3` | `cca65debbbdf02863111cd9f8ea1fde1c2f34555` |

The US profile comes from the AC6_recomp reference. The Europe profile was translated from it with Xenon's `hint-port`, which matches code between the two executables. Each address was checked against the Europe executable. **US addresses are never applied to another disc directly.**

`gracemeria-export-analysis` (build target `gracemeria-xenon-analysis`) writes every profile into `xenon-analysis/`. The launcher prepares the module for whichever disc the player has from that one package.

## Where AC6 fixes and enhancements go

As work on AC6 finds something the game needs, it goes in the revision profile, never into Xenon. Missing Xbox behaviour is the exception: it is fixed in Xenon for every game.

- **Instruction hooks.** These handle a game bug or quirk at specific guest instructions. Declare the hook in `analysis/hooks.json` (`enabled`, with a `classification`). Write its routine in `hooks/*.cpp` as `void name(xenon::cpu::ExecutionContext&)`, or `bool` to choose between two actions. Preparation compiles it into that disc's module. The 12 `ac6StorageOrderHook*` hooks in `usa-retail-tu0` are the working example. They reproduce the title update's job-submit ordering fix, which Europe Rev 1 already contains.
- **Rendering data.** `rendering/rendering.json` holds Xenon's temporal rendering profile: AC6's camera constants, lit-scene target and HUD shaders. Xenon reads it at launch for the running disc, and it is the input for FSR, XeSS, DLSS and frame generation later. It is left empty until AC6 renders; Xenon's temporal probe (`"temporalProbeFrames"` in a launch config) then measures the values. See Xenon's `docs/graphics/TEMPORAL_RENDERING.md`.
- **Hints.** `analysis/hints.v2.json` and `hints.json` cover function starts, indirect calls and helpers Xenon's analysis misses.

After changing a profile, rebuild `gracemeria-xenon-analysis`. The `gracemeria_xenon_analysis_tests` test fails while `xenon-analysis/` is out of date.

## What was migrated from AC6_recomp

`ac6recomp_config.toml` was audited in full. The supplied file contains 10,527 function records, two known indirect-call sites, 16 ReXCRT/native helper candidates, setjmp/longjmp addresses and 34 midasm hooks.

The active Xenon hint set is deliberately much smaller than the legacy table. Xenon automatic discovery owns ordinary function boundaries. The full legacy symbol map is retained as `analysis/legacy-symbols.csv`, and `tools/reduce_legacy_hints.py` can compare it with a real Xenon analysis report to keep only starts that Xenon actually misses.

ReXGlue runtime plumbing, the vendored ReXGlue/Xenia renderer, duplicate Xbox services and backend-specific D3D hooks are not part of Gracemeria. See `docs/AC6_RECOMP_AUDIT.md`.

## Build the migration tools

Requirements: CMake 3.25+, a C++20 compiler, Ninja (recommended), Python 3 for metadata tests, and the current Xenon source tree.

```bash
cmake -S . -B build/dev -G Ninja \
  -DXENON_RECOMP_ROOT=/path/to/Xenon-Recomp \
  -DGRACEMERIA_BUILD_TESTS=ON
cmake --build build/dev
ctest --test-dir build/dev --output-on-failure
```

### Generate AC6 native code

With a legally supplied supported `default.xex`:

```bash
./build/dev/gracemeria-recomp /path/to/default.xex generated/ac6-us
```

Xenon's Recomp Driver now accepts title-update input through the same XEX Loader V2 path used by the runtime. Gracemeria deliberately rejects an effective title-update image until that update has its own verified revision profile; this prevents TU0 addresses from leaking onto a patched executable. Once a profile is verified, generation uses:

```bash
./build/dev/gracemeria-recomp /path/to/default.xex generated/ac6-us-tu \
  --title-update /path/to/title-update.xexp
```

Historical public metadata identifies a TU2 for Media ID `531C30BE`, but Gracemeria does not guess its effective XEX version/hash or reuse TU0 addresses. Xenon owns XEXP parsing, validation and effective-image construction.

The generated directory contains Xenon's normal `xenon_game` and `xenon_game_module` targets. Build it with:

```bash
cmake -S generated/ac6-us -B build/generated/ac6-us \
  -DXENON_RECOMP_ROOT=/path/to/Xenon-Recomp
cmake --build build/generated/ac6-us --target xenon_game_module
```

Then stage an installable module directory with `tools/stage_package.py`.

## Import coverage

After building the tools, run:

```bash
./build/dev/gracemeria-import-coverage /path/to/default.xex
```

The report lists every XEX import as `library`, `ordinal`, `name`, and current Xenon status: `registered`, `stub`, or `missing`. It queries Xenon's real `XenonSession`/`ExportRegistry`; Gracemeria does not maintain a duplicate import-status database.

## Title updates and DLC

Xenon owns title updates, STFS packages and mounted content. Gracemeria only declares compatibility and identity metadata. The US base executable lacks the title update's storage-submit ordering fix, so `usa-retail-tu0` applies that one fix as instruction hooks. Europe Rev 1 shipped with it, so its hooks are recorded as `fixed-in-revision` and stay off.

The installed `xenon-module.json` carries the current 65-entry AC6 DLC catalogue so Xenon Launcher can present/import known add-ons through its normal `ModuleService::dlcCatalog()` path. These catalogue records are descriptive only until their STFS headers are verified: `contentIds` remain empty and `identityStatus` remains `pending-stfs-header-verification` rather than inventing package identities.

DLC packages, title updates, disc images and game assets are never distributed here. DLC content IDs are promoted to strict identities only after verification from the user's own legally supplied packages.

## Optional enhancements

AC6_recomp includes useful research for 60 FPS, cutscene resync, widescreen, full-resolution effects, terrain quality and keyboard/mouse. Their current implementations are coupled to ReXGlue/Xenia backend internals, so they are preserved as classified research rather than copied into Xenon's production graphics/input/audio paths. They can be reimplemented later against stable Xenon extension points without creating a second renderer or input system.

## Research and credits

Project Gracemeria benefits from public research and implementation experience from AC6_recomp, ReXGlue, Xenia, XenonRecomp, XenosRecomp, Unleashed Recompiled and the Dead Rising 2 Case Zero/Case West Xenon recompilation projects. `docs/RESEARCH_AND_LICENSES.md` distinguishes reference research from source copied into this repository.

No source code from AC6_recomp/ReXGlue/Xenia was copied into this migration. Address/name metadata from the public AC6 configuration was translated into Gracemeria data files and Xenon's module-hint model.

## Legal

Project Gracemeria is an unofficial fan project and is not affiliated with or endorsed by Bandai Namco Entertainment, Project Aces or Microsoft.

No copyrighted Ace Combat 6 game executable, disc image, assets, DLC, title update, Microsoft keys or firmware are included. Users must supply their own legally obtained content locally.

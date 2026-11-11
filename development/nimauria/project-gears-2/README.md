# Project Gears 2

A [Project Xenon](https://github.com/nimauria) game module for **Gears of War 2** (Xbox 360). Xenon translates the
game's own PowerPC code to native code from your disc and supplies the Xbox 360 runtime: kernel, filesystem,
graphics, audio and input. This module supplies only what is specific to Gears 2: which executable revisions are
supported, analysis hints for them, and game-specific hooks.

No game data is included. You need your own copy of the game.

## Supported revisions

| Revision | Media ID | Executable | Status |
|---|---|---|---|
| Europe/Australia retail disc (`revisions/eur-retail-51d281be`) | 51D281BE | version 0x00000007, effective image hash `dc3ba9d7…5c89` | identity verified; in development |

Its code is byte-identical to the US 0.0.0.5 disc executable that COGBox targets, so COGBox's disc-build findings
apply to it at the same addresses. Title Update 6 is a different executable and will get its own revision.

## Layout

- `xenon-module.json`: the module manifest the launcher reads.
- `revisions/<revision>/`: everything about one executable revision:
  - `revision.json`: its identity (title, media, versions, Xenon effective image hash);
  - `analysis/hints.json`: function bounds and other analysis hints, each with its source;
  - `analysis/hooks.json`: instruction hooks, and `hooks/*.cpp` their C++;
  - `content/`, `compatibility/`: content and patch metadata.
- `xenon-analysis/`: the package Xenon's game preparation reads, generated from `revisions/` by
  `tools/export_xenon_analysis.py`. Never edit it by hand.
- `tools/`: the exporter and `validate_metadata.py`.
- `tests/`: hook unit tests (`*_hook_tests.cpp`), built against Xenon's CPU library.
- `runtime/`: launch configurations, sessions, saves and screenshots for local runs (sessions and saves are not
  committed).
- `docs/`: architecture and research notes.

## Working on it

```
python tools/export_xenon_analysis.py      # after editing revisions/
python tools/validate_metadata.py
cmake -S . -B build -DXENON_RECOMP_ROOT=<Xenon-Recomp> && ctest --test-dir build
```

Game preparation (from the Xenon-Recomp build directory):

```
xenon-prepare --content "<extracted game folder>" --cache-root <cache> \
  --module "<this folder>/xenon-analysis" --module-id org.nimauria.project-gears-2 \
  --recomp-root <Xenon-Recomp> --config Release
```

## Licence

MIT; see `LICENSE`. Research sources and their licences are listed in `docs/RESEARCH_AND_LICENSES.md`.

# Game development workspace

Keep authored source, hooks, analysis inputs, revision mapping, tests, build
projects and title-specific tools under <username>/<project>/. Shared generic
runtime changes belong in Xenon-Recomp. The root .gitignore owns the merged
project rules; do not add broad ignore patterns that hide files under games/.

Current projects:

| Project | Development folder | Distribution |
|---|---|---|
| Project Gracemeria | nimauria/project-gracemeria | Existing preparation payload under games/nimauria/project-gracemeria |
| Project Gears 2 | nimauria/project-gears-2 | Development only; payloadPath is null |

The original working trees have been preserved. Current tracked and non-ignored
working files were imported without their .git folders or generated build trees.
Their licences and research attribution remain with each project. Empty Gears 3
and Judgment folders were not turned into fictional projects or catalogue entries.

Each project.json lists files to export. Its preparationFiles are used only when
there is no prebuilt DLL. The export script does not automatically publish or
commit anything. A development-only project needs a reviewed entry.json and a
matching payloadPath before it can be exported into games/.

Use each project's CMakeLists.txt with an explicit XENON_RECOMP_ROOT. Imported
runtime launch configurations and trace scripts are historical development
examples and may contain old machine-specific paths; update those local paths
before using them. Never move extracted game content into this repository.

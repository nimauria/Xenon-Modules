# Xenon Modules

The shared home for Xenon game modules. Contributors keep their games under
their username and submit changes to this repository:

~~~text
games/
  nimauria/
    project-gracemeria/
      entry.json            # Discovery and presentation
      xenon-module.json     # Installed runtime manifest
      package-files.json    # Paths to install
      package.json          # Generated sizes and SHA-256 hashes
      assets/
      xenon-analysis/
  another-contributor/
    their-game/
~~~

One contributor could maintain games/nimauria/gears-of-war/ through
gears-of-war-judgment/. These are layout examples, not available games.

Xenon Launcher reads catalog.json and each game's entry.json and package.json
from this repository's main branch. It downloads only the selected game's
indexed files and verifies every size and SHA-256. There are no per-game
repositories, release tags, or uploaded release packages to maintain.
Merged file changes are detected even without a manifest version bump.

The launcher assembles a temporary archive locally to reuse staged installation,
identity validation, previous-version retention, and rollback. Each module keeps
its own ID, manifest, settings, and compatibility data. Game folders contain
installable payloads: DLLs, details, artwork, licences and required runtime data.
Developer source, tests, build projects and tools are excluded. Hooks already
compiled into a prebuilt DLL do not need separate source files. Gracemeria still
uses its exported preparation data until a tested prebuilt DLL is available.

## Contributing

See [submission instructions](docs/MODULE_SUBMISSION.md) and the
[format contract](docs/REGISTRY_FORMAT.md). With Node.js 24 or newer:

~~~sh
node scripts/sync-packages.mjs
node scripts/validate-catalog.mjs
~~~

Commit generated package indexes alongside the files they describe. CI rejects
stale indexes, duplicate IDs, unlisted game folders, and invalid paths.

No commercial game files, Xbox executables, updates, DLC, firmware or keys belong
here. Users supply their own content. Game content stays local; module DLLs can be distributed here.
See [security](docs/SECURITY.md) and each module's licence and research credits.

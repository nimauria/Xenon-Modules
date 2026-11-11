# Xenon Modules

One shared repository for game development projects and their release payloads.

~~~text
development/<username>/<project>/   Source, hooks, tools, tests and revision work
games/<username>/<game>/           Exported files ready for distribution
  entry.json                      Discovery and compatibility details
  xenon-module.json                Runtime manifest
  package-files.json               Explicit distribution file list
  package.json                     Generated sizes and SHA-256 hashes
  bin/windows-x64/module.dll       Built native module, when available
  runtime/                        Required external runtime data, when needed
  assets/                         Artwork permitted for redistribution
  artwork.json                    Artwork provenance and attribution
.local/                           Ignored personal data and unconfirmed artwork
~~~

Contributors can maintain several games under their username. Development files
are ordinary files in this repo, not nested repositories. The launcher reads
only the catalogue and exported game payloads; it never downloads development/.
Game content, generated game code, caches and build outputs stay local.

Project Gracemeria and Project Gears 2 have been imported into development/nimauria/.
Gears 2 remains development-only and has no public catalogue entry. Gracemeria's
existing preparation package remains in games/; no prebuilt DLL is supplied or
claimed tested. A package's presence does not establish retail-game playability.

The root .gitignore merges the old project rules under their development paths.
Development DLLs and caches are ignored; explicitly exported release DLLs and
permitted artwork under games/ remain trackable.

## Development and export

Use Node.js 24 or newer. Each development project's project.json declares its
module ID, distribution destination and exact export inputs. Work under
development/, then explicitly export a reviewed package:

~~~sh
node scripts/export-module.mjs development/nimauria/project-gracemeria
node scripts/export-module.mjs development/nimauria/project-gracemeria --check
node scripts/validate-catalog.mjs --publish
node --test tests/workspace.test.mjs
~~~

For a tested prebuilt DLL, supply its project-relative path and host platform:

~~~sh
node scripts/export-module.mjs development/nimauria/project-gracemeria --binary build/release/xenon_game_module.dll --platform windows-x64
~~~

That places the DLL under bin/windows-x64/, declares nativeExtension, and omits
preparation hook source from the package. Hooks compiled into the DLL do not
need separate source at runtime. Additional runtime dependencies must be listed
explicitly in project.json. Build tooling continues to use an explicit
XENON_RECOMP_ROOT pointing to the generic Xenon-Recomp SDK/runtime checkout.

The launcher downloads indexed files directly from main and verifies each hash.
Modules need no separate game repository, GitHub Release or uploaded ZIP.
File changes expose updates even without a version bump. Commit payload changes
and their generated package index together.

## Supplied artwork

Artwork is optional. You can supply original, licensed or permission-backed
images using the [artwork workflow](docs/ARTWORK.md). Attribution alone does not
establish permission. Current publisher images with unconfirmed permissions
have been preserved under ignored .local/artwork/ and removed from the new
public payload. The launcher shows its existing fallback when art is absent.

See [development](development/README.md), [submissions](docs/MODULE_SUBMISSION.md),
[package format](docs/REGISTRY_FORMAT.md), and [security](docs/SECURITY.md).

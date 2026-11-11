# Workspace migration

Project Gracemeria and Project Gears 2 development files were imported into
development/nimauria/. The copy includes the original tracked files and current
non-ignored working changes. Their original working trees and Git histories were
left intact; no nested .git directories or build caches were imported here.

Per-project .gitignore files were merged into the root ignore file with their
scope preserved. No developer ignore rule applies to published DLLs or art in
games/. Existing licences, source provenance and research credits were retained.

The existing Gracemeria payload keeps its ID and supported revision data. It
still uses local preparation; no tested prebuilt DLL is available in this import.
Gears 2 has no catalogue entry and remains development-only. The other empty
Gears project folders were not imported as supported games.

Publisher cover/background images with unconfirmed redistribution rights were
preserved locally in ignored .local/artwork/ and removed from the new payload.
Their artwork.json records remain local-only. Public history was not rewritten.

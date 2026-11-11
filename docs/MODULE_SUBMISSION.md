# Contributing a module

1. Add source, tools, hooks, tests and revision work to development/<username>/<project>/.
   Keep generated game code, game content and build caches out of Git. Use the
   shared root ignore rules; add project-specific rules scoped to development/.
2. Add project.json following schema/development-project.schema.json. Use a
   stable module ID matching xenon-module.json. Leave payloadPath null while the
   project is not ready for distribution.
3. When ready, create games/<username>/<game>/entry.json and the initial runtime
   manifest and package-files.json. Match entry.id and developmentPath to the
   development project. Set the project's payloadPath to this game folder.
4. Declare supported hosts and accurate compatibility. Put native libraries under
   bin/<platform>/. Compile hook implementations into a prebuilt DLL. List any
   required external runtime data explicitly. Preparation packages may include
   exported analysis and hook inputs when Xenon builds their DLL locally.
5. Artwork is optional. Use scripts/import-artwork.mjs with a complete permission
   record for public images. Unconfirmed publisher artwork stays under ignored
   .local/artwork/. See ARTWORK.md.
6. Explicitly export reviewed runtime files with scripts/export-module.mjs, then
   run scripts/validate-catalog.mjs --publish and the project's own checks. Commit
   exported files and their generated package.json together. Development edits
   are not automatically released to launcher clients.
7. Submit a PR here. No per-game repository, GitHub Release or uploaded ZIP is
   required. Include validation evidence and remaining compatibility limits.

The launcher reads only catalogue entries and their declared games/ payloads.
DLLs are ignored in development/ and intentionally trackable in games/. Publishers
remain responsible for rights to distribute every file, including artwork and
module binaries. A folder or publisher label does not prove permission.

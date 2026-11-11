# Contributing a game module

1. Create games/<github-username>/<game-slug>/ using lowercase names.
2. Put only the installable module DLLs, metadata, licence and artwork there.
   Keep its module ID stable across moves and updates. Several games can share
   one contributor folder. A fork of an existing module needs a distinct ID.
3. Add entry.json following schema/module-entry.schema.json: version 2,
   sourcePath matching the folder, repository pointing to nimauria/Xenon-Modules,
   and supported host keys in platforms. Artwork paths begin with
   <sourcePath>/assets/. Preserve accurate author attribution in publisher.
4. Add xenon-module.json with the same ID, a semantic version, supported game
   identities, and accurate compatibility requirements.
5. Add package-files.json with an include array of module-relative files or
   directories. Include xenon-module.json, licences, runtime metadata,
   and required artwork. Exclude developer source, tests, build tools and user game data.
   For prebuilt DLLs, hook implementations are already compiled in. Include
   preparation inputs only when the module explicitly builds its DLL locally.
6. Add <sourcePath>/entry.json to catalog.json.
7. Run node scripts/sync-packages.mjs and node scripts/validate-catalog.mjs.
   Commit package.json alongside the files it indexes. Validate the module DLL
   against its declared Xenon ABI and supported game revisions before publishing.
8. Submit a pull request here. After review and merge, launcher clients can
   discover and install the module. No separate repository or release is needed.

For updates, edit the game folder and regenerate its index. A version bump is
useful for people; file digests determine whether installed content has changed.
Other games remain independent. Platform declarations are not playability
evidence. Include test results and remaining limitations in pull requests.

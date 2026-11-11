# Repository module format

catalog.json uses xenon.module-catalog version 2. Its modules array lists
games/<contributor>/<game>/entry.json paths (at most 256). Paths and stable IDs
must be unique; every game folder has one entry.

entry.json uses xenon.module-entry version 2. It declares identity, publisher,
licence, tags, presentation, platforms and sourcePath. The repository is always
nimauria/Xenon-Modules. Runtime details belong in xenon-module.json alongside it.

package-files.json declares {"include":["xenon-module.json","LICENSE",...]}.
The sync script expands those paths into package.json:

~~~json
{
  "schema": "xenon.repository-package",
  "version": 1,
  "moduleId": "org.contributor.game",
  "moduleVersion": "0.1.0",
  "files": [{"path":"xenon-module.json","size":123,"sha256":"64 lowercase hex digits"}]
}
~~~

The index contains sorted unique module-relative paths, exact sizes, and hashes.
The runtime manifest is required. Limits: 20,000 files, 128 MiB total, 240-byte
paths. ASCII letters, digits, dots, underscores and hyphens are allowed in path
components; parent traversal and symlinks are forbidden. CI regenerates indexes
in memory and rejects mismatches.

The launcher reads https://raw.githubusercontent.com/nimauria/Xenon-Modules/main/
and appends sourcePath and each indexed path. Content identity is SHA-256 of
concatenated records: path + LF + decimal size + LF + lowercase hash + LF,
in index order. This identity is retained with the installed module and its
rollback history. Equal versions do not hide source edits.

Missing or changed files abort download and preserve the installation. Refresh,
check and retry after the repository settles. Launcher self-updates continue
using Xenon-Recomp releases. Module installation does not use GitHub Releases.

Updated launchers accept version-1 discovery entries during migration and map
their repository owner/name to lowercase games/<owner>/<name> paths. Old
launchers cannot read version 2: ship the new launcher before changing the public
catalogue. Existing module IDs are retained.

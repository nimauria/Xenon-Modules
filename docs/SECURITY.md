# Module trust and integrity

Official discovery and package files come from reviewed commits in
nimauria/Xenon-Modules under games/<contributor>/<game>/. Contributor folders
group projects and preserve attribution. They grant no automatic publishing or
verified-publisher privilege. Changes remain subject to repository review.

The launcher validates paths, IDs, platforms, sizes and package limits. Each file
is checked against its package index. A locally assembled archive is checked
again before installation, and the installed manifest must match the selected
ID and version. Failed updates retain the installation. Previous installations
retain their content digests for rollback.

Hashes detect corruption and files changing during download. They do not
authenticate code independently of the repository: files and hashes share a
trust source. Modules can contain native hooks and must be reviewed as code.
The launcher does not execute module code during installation.

Commercial game payloads, firmware, executables, title updates, DLC and keys must
not be committed. Game content stays local; module DLLs can be distributed here. Signatures,
revocation and finer contributor permissions remain future work.

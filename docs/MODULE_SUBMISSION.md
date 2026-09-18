# Submitting a Xenon Module

The Xenon module registry is intended to discover independently maintained Xenon-compatible modules.

## Expected submission process

A module should normally:

1. Have its own public source repository.
2. Contain a valid Xenon module manifest.
3. Publish at least one compatible release package.
4. Use a stable module ID.
5. Add a registry entry under `modules/`.
6. Add that entry to `catalog.json`.
7. Pass automated registry validation.
8. Be submitted through a pull request.

## Module IDs

Module IDs should use reverse-domain-style identifiers.

Example:

~~~text
org.nimauria.project-gracemeria
~~~

The module ID used by the registry must match the ID contained in the installed module package.

## Commercial content

Registry submissions must not contain or distribute commercial game files, Xbox 360 executables, title updates, DLC, firmware, keys, or other proprietary content.

The registry points only to Xenon-compatible module software and metadata.

## Status

The submission process is still being designed and may change before the first stable registry specification.

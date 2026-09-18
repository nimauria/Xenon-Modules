# Xenon Module Registry Format

The Xenon Modules repository provides discovery metadata for modules supported by Xenon Launcher.

The registry is deliberately separate from the runtime module manifest.

## Catalog

`catalog.json` is the entry point used by Xenon Launcher.

Current schema:

~~~text
xenon.module-catalog
~~~

Current version:

~~~text
1
~~~

The `modules` array contains repository-relative paths to individual registry entries.

Example:

~~~json
{
  "schema": "xenon.module-catalog",
  "version": 1,
  "modules": [
    "modules/org.nimauria.project-gracemeria.json"
  ]
}
~~~

## Module entries

Each module has a separate JSON file under `modules/`.

Registry entries contain only information required for discovery and distribution, including:

- module ID
- display name
- description
- publisher
- source repository
- supported release assets
- tags

Runtime-specific information belongs to the installed module manifest rather than the public registry.

This includes:

- supported Xbox title IDs
- executable hashes
- game versions
- runtime capabilities
- DLC definitions
- launch configuration
- generated code
- hooks and patches
- module-specific settings

## Stability

The registry format is currently under development.

Schema version `1` should be treated as an initial contract and may evolve while the Xenon module architecture is being completed.

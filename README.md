# Xenon Modules

**Xenon Modules** is the official public module registry and discovery catalogue for the [Xenon Recomp](https://github.com/nimauria/Xenon-Recomp) ecosystem.

The registry allows Xenon Launcher to discover compatible game modules, identify where they are published, locate supported release packages, and determine which modules are available for a user's platform.

This repository does **not** contain commercial game files, Xbox 360 executables, title updates, DLC, firmware, encryption keys, or other proprietary content.

It contains only open module-registry metadata, schemas, documentation, and validation infrastructure.

---

## Purpose

Xenon Recomp is designed around a shared runtime and launcher capable of supporting multiple independently developed Xbox 360 static-recompilation projects.

Rather than hard-coding supported games into Xenon itself, each title can be implemented as its own **Xenon module**.

The registry provides the discovery layer between the Xenon Launcher and those independently maintained modules.

```text
                    Xenon Launcher
                          |
                          v
                   Xenon Modules
                  Public Registry
                          |
              +-----------+-----------+
              |                       |
              v                       v
        Game Module A             Game Module B
        GitHub Repository         GitHub Repository
              |                       |
              v                       v
        GitHub Releases           GitHub Releases
              |                       |
              +-----------+-----------+
                          |
                          v
                    Xenon Launcher
                       installs
                       module
                          |
                          v
                     Xenon Runtime
```

The registry tells Xenon **where a module can be found**.

The module itself remains responsible for its own runtime metadata, compatibility information, supported game versions, settings, generated code, hooks, patches, and other title-specific behaviour.

---

# Registry vs Module Manifest

The public registry and an installed module's manifest serve different purposes.

## Registry entry

A registry entry contains discovery and distribution information such as:

- stable module ID;
- module name;
- short description;
- publisher;
- source repository;
- release location;
- supported host platforms;
- package filenames;
- licence information;
- verification status;
- search/category tags.

For example:

```text
Module ID:
org.nimauria.project-gracemeria

Repository:
nimauria/Project-Gracemeria

Windows x64 package:
project-gracemeria-windows-x64.xenonmod.zip
```

## Module manifest

The installed module owns game/runtime information such as:

- supported Xbox title IDs;
- supported executable versions;
- supported regions;
- runtime requirements;
- generated recompilation code;
- hooks and patches;
- module capabilities;
- module-specific settings;
- DLC definitions;
- compatibility state;
- launch configuration;
- game-specific services or adapters.

These details deliberately remain outside the central registry.

This allows the registry format to stay stable even while Xenon's game-module ABI evolves.

---

# Distribution model

Xenon Modules is a **registry**, not a binary hosting repository.

Game modules are developed and released independently.

```text
Module developer
      |
      v
Module GitHub repository
      |
      v
GitHub Release
      |
      v
.xenonmod.zip package
      |
      v
Xenon Modules registry entry
      |
      v
Xenon Launcher
```

A module therefore does not need to be uploaded to this repository every time it is updated.

The registry points Xenon Launcher toward the module's own release source.

This lets every game project:

- maintain its own source repository;
- use its own development workflow;
- publish releases independently;
- release on its own schedule;
- maintain its own changelog;
- version its module independently from Xenon Recomp.

---

# Module discovery

The intended launcher flow is:

```text
Xenon Launcher
      |
      v
Fetch registry
      |
      v
Read available modules
      |
      v
Filter for current platform
      |
      v
Display Module Catalog
      |
      v
User selects module
      |
      v
Read module release
      |
      v
Download matching package
      |
      v
Verify package
      |
      v
Stage installation
      |
      v
Install / update module
```

The same registry will also provide the metadata used by the launcher's module search and discovery interface.

---

# Module updates

Module updates remain independent from Xenon Launcher updates.

```text
Xenon Launcher update
        |
        v
nimauria/Xenon-Recomp
        |
        v
GitHub Releases


Game module update
        |
        v
Xenon Modules registry
        |
        v
Module repository
        |
        v
GitHub Releases
```

This means updating a game module does not require publishing a new version of Xenon itself.

The updater is expected to:

1. identify the installed module;
2. resolve its registry entry;
3. locate the module's configured repository;
4. inspect available releases;
5. select the appropriate platform package;
6. verify the release/package;
7. stage the update;
8. replace the installed module safely.

---

# Package integrity

Module installation should never blindly trust a downloaded archive.

The Xenon module infrastructure is intended to support verification such as:

- expected release asset names;
- SHA-256 package digests;
- registry/schema validation;
- module-ID verification;
- package manifest validation;
- publisher/repository matching;
- supported-platform validation.

Additional trust and signing mechanisms may be added as the ecosystem develops.

---

# Registry trust

The official registry may eventually distinguish between different levels of module trust.

For example:

```text
Verified
  Module maintained by a recognised or reviewed publisher.

Community
  Third-party module listed in the registry.

Development
  Experimental or prerelease module.
```

The exact trust model has not yet been finalised.

The launcher should clearly communicate module provenance rather than implying that every listed module is maintained by the Xenon Recomp project.

---

# Adding modules

The expected long-term contribution model is:

```text
Create Xenon-compatible module
        |
        v
Publish source repository
        |
        v
Publish first module release
        |
        v
Add registry entry
        |
        v
Open pull request
        |
        v
Automated validation
        |
        v
Review
        |
        v
Merge
        |
        v
Module becomes discoverable
```

Detailed submission requirements will be documented once the registry schema and module packaging specification are finalised.

---

# Planned repository structure

The exact structure is still being designed, but the intended direction is similar to:

```text
Xenon-Modules/
├─ README.md
│
├─ catalog.json
│
├─ modules/
│  └─ org.nimauria.project-gracemeria.json
│
├─ schema/
│  ├─ catalog.schema.json
│  └─ module-entry.schema.json
│
├─ docs/
│  ├─ MODULE_SUBMISSION.md
│  ├─ REGISTRY_FORMAT.md
│  └─ SECURITY.md
│
└─ .github/
   └─ workflows/
      └─ validate-catalog.yml
```

This structure may change while the registry specification is being developed.

---

# First module

The first intended module in the registry is:

**Project Gracemeria**

Repository:

[github.com/nimauria/Project-Gracemeria](https://github.com/nimauria/Project-Gracemeria)

Project Gracemeria is the first real-title integration target for Xenon Recomp and is intended to provide the Xenon module for **Ace Combat 6: Fires of Liberation**.

Ace Combat 6-specific implementation details remain within Project Gracemeria rather than the generic Xenon runtime or central registry.

---

# Relationship with Xenon Recomp

The project architecture deliberately separates three major components.

## Xenon Recomp

The reusable Xbox 360 native recompilation runtime.

It provides shared infrastructure such as:

- Xenon PPC/VMX128 CPU support;
- Xenon IR and native AOT recompilation;
- Xbox 360 memory semantics;
- Xenos graphics processing;
- Vulkan and Direct3D 12 host backends;
- filesystem/runtime services;
- launcher infrastructure.

Repository:

[github.com/nimauria/Xenon-Recomp](https://github.com/nimauria/Xenon-Recomp)

## Xenon Modules

This repository.

It provides:

- public module discovery;
- registry metadata;
- module source/release locations;
- schema definitions;
- validation infrastructure.

## Game modules

Independent title-specific projects.

They provide:

- recompiled game code;
- hooks and patches;
- supported game/version definitions;
- module manifests;
- title-specific compatibility behaviour.

The dependency direction should remain:

```text
Game Module
     |
     v
Xenon Runtime

Xenon Runtime
     X
     |
No dependency on an individual game module
```

---

# Legal

Xenon Modules is an independent open-source project.

It is not affiliated with, endorsed by, or sponsored by Microsoft, Xbox Game Studios, Bandai Namco, Project Aces, Epic Games, or other publishers or developers whose software may eventually be supported by independent Xenon modules.

Xbox, Xbox 360, Xbox Live, and related names are trademarks of Microsoft Corporation.

No proprietary Xbox 360 firmware, executables, commercial game data, title updates, DLC, encryption keys, or other copyrighted game content are distributed through this repository.

Game modules listed by the registry are expected to operate only on content legally obtained and supplied by the user.

---

# Status

Xenon Modules is currently in **early development**.

The registry schema, package format, validation workflow, trust model, and Xenon Launcher integration are being designed alongside the wider Xenon Recomp module architecture.

The format should be considered unstable until the first registry specification is finalised.

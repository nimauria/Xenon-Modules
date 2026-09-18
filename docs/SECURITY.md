# Xenon Modules Security

Module discovery and installation cross a trust boundary between the Xenon Launcher and independently maintained module repositories.

A downloaded module package should never be trusted solely because its URL appears in the public registry.

## Current security model

Registry infrastructure is being designed around:

- schema validation
- stable module IDs
- repository identity
- expected release asset names
- platform validation
- module manifest validation
- SHA-256 release-asset verification
- staged installation

## Future work

Future versions may introduce additional mechanisms such as:

- publisher signing
- package signatures
- registry signatures
- repository ownership verification
- stronger publisher trust levels
- revocation metadata

The exact long-term trust model has not yet been finalised.

## Commercial content

The Xenon module registry must not be used to distribute proprietary Xbox 360 game content.

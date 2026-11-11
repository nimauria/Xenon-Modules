# Architecture

Gracemeria has two phases.

1. **Source/codegen phase**: `Ac6HintProvider` validates AC6 revision identity and supplements Xenon's Recomp Driver. Xenon parses the XEX, optionally constructs a title-update effective image, performs CPU V2 analysis and emits the native compiled registry.
2. **Runtime phase**: Xenon Launcher installs the module package. XenonRuntime/XenonSession loads the generated native extension and resolves `Xenon_BindCompiledRegistry`. All Xbox services remain Xenon-owned.

The hint provider is a build-time component; it is not a competing runtime host.

### Revision safety

Fixed addresses are never global AC6 constants. They live under one revision profile. A mismatch is a hard error in the provider. PAL and title-update effective images need their own verified profiles before receiving fixed-address knowledge.

### Title-update provenance

`XexImage::title_update_applied` is retained by Xenon after effective-image construction. This is necessary because a reconstructed title image may no longer carry patch module flags. Gracemeria uses that provenance to reject unverified effective revisions instead of accidentally treating them as TU0.

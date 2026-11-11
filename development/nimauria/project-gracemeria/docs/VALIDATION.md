# Real-title validation

The source migration does not require copyrighted game content. The following validation does:

1. Inspect the legally supplied `default.xex` with Xenon's loader and record title ID, media ID, XEX version and effective-image SHA-1.
2. Confirm it is Media ID `531C30BE` before enabling the migrated US fixed-address profile.
3. Run `gracemeria-recomp` and save Xenon's `analysis.json`.
4. Run `tools/reduce_legacy_hints.py` to identify legacy starts Xenon does not discover automatically.
5. Run `gracemeria-import-coverage` and triage only actual missing/stubbed imports.
6. Build `xenon_game_module` and validate `Xenon_BindCompiledRegistry` through XenonSession.
7. If using a title update, validate/apply it through Xenon, record the effective XEX version/hash, and create a separate effective-revision profile before enabling any address-scoped symbols/hooks. Until then the provider rejects the effective image.
8. Confirm Xenon Launcher exposes the 65 installed-manifest DLC definitions through `ModuleService::dlcCatalog()`.
9. Verify individual DLC identities from locally supplied STFS headers; do not invent content IDs. Promote a catalogue entry from `pending-stfs-header-verification` only after its 20-byte Content ID is confirmed.

No step requires committing the user's executable, update or DLC data.

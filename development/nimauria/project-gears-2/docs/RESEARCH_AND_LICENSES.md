# Research and licences

## COGBox (Gears2-Native)

- Source: Shagg88/COGBox, MIT licence (Copyright (c) 2026 Shagg). Local copy:
  `Documents/Development/Xbox360-Recomp/COGBox_reference/Gears2-Native`.
- What it is: native ports of the Gears games built with the rexglue static recompiler, with their own renderer
  and launcher.
- Which executable: its launcher fingerprints Gears 2 by the US disc `default.xex` (18,595,840 bytes, SHA-1
  `b6860bf31e55013467b53eca963d12314c0d78da`, code SHA-1 `eb8aedd4567c220176ae4e7d95c89f667a361387`) and Title
  Update 6 (`default.xexp`, 2,074,624 bytes). From 0.4.0 it requires TU6.
- Our Europe/Australia disc executable (media 51D281BE, SHA-1 `e39b0eafee460fa699f52e06860f65700026b7c3`) has the
  same size and the same code SHA-1, `eb8aedd4…`: only the header (media ID, version) differs. So:
  - COGBox's **disc-build** hints (`gow2_manifest.toml`) name the same code at the same addresses here;
  - its **TU6** hints (`gow2_manifest_tu6.toml`) are for a different executable.
- How it is used: as research. Function bounds and findings are re-verified against Xenon's analysis of this
  executable and recorded with their source (COGBox job/note) in `revisions/*/analysis/`. rexglue-specific
  runtime behaviour is not copied: Xbox semantics belong in Xenon.

## Project Xenon

- Xenon owns PPC, memory, kernel, XAM, filesystem, audio and Xenos semantics. A gap Gears 2 exposes in any of
  those is fixed in Xenon, not here.

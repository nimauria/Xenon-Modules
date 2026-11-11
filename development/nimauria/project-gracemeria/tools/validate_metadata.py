#!/usr/bin/env python3
from __future__ import annotations
import csv, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Every revision profile: its directory, media ID, region, how many hooks it
# classifies, and how many hooks of each enabled class it must enable. US TU0
# lacks the title update's storage-order reordering (Europe Rev 1 already
# contains it) and carries the host-timing fixes; Europe's are not ported yet.
REVISIONS = [
    ("usa-retail-tu0", "531C30BE", "NTSC-U", 38, {"base-executable-fix": 12, "host-timing-fix": 4}),
    ("eur-retail-rev1", "0379EFB3", "PAL", 34, {}),
]
# Hook classes that may run: title-update fixes, and fixes for races and
# clock drift a PC host exposes. Enhancements and research probes may not.
ENABLED_CLASSES = ("base-executable-fix", "host-timing-fix")

def fail(msg: str):
    raise SystemExit(f"metadata validation failed: {msg}")

def check_revision(manifest, directory: str, media: str, hook_count: int, enabled_expected: dict) -> str:
    rev_dir = ROOT / "revisions" / directory
    def load(name: str):
        with (rev_dir / name).open(encoding="utf-8") as f:
            return json.load(f)
    rev = load("revision.json")
    hints = load("analysis/hints.json")
    hooks = load("analysis/hooks.json")
    repl = load("analysis/native-replacements.json")
    patches = load("compatibility/patches.json")
    content = load("content/content.json")
    where = f"{directory}: "

    if rev["identity"]["titleId"] != "4E4D07D1": fail(where + "revision title")
    if rev["identity"]["mediaIds"] != [media]: fail(where + "revision media")
    if not re.fullmatch(r"[0-9a-f]{40}", rev["identity"].get("effectiveImageHash") or ""):
        fail(where + "revision needs its effectiveImageHash (the xenon-analysis package is keyed by it)")
    if content["mediaId"] != media: fail(where + "content media")
    if hints["revision"] != rev["id"]: fail(where + "hint revision scope")
    if len(hints["indirectCallSites"]) != 2: fail(where + "indirect calls")
    if len(hints["knownSymbols"]) != 18: fail(where + "active symbol count")
    if len(repl["mappings"]) != 16: fail(where + "native replacement candidates")
    if any(m.get("enabled") for m in repl["mappings"]): fail(where + "generic replacements must not be module-owned")
    if len(hooks["hooks"]) != hook_count: fail(where + "hook classification coverage")
    # Only fixes run by default; every one needs its Xenon instruction-hook
    # routine in the revision's hooks/.
    enabled = [h for h in hooks["hooks"] if h.get("enabled")]
    if any(h["classification"] not in ENABLED_CLASSES for h in enabled):
        fail(where + "only base-executable-fix and host-timing-fix hooks may be enabled")
    for classification in ENABLED_CLASSES:
        count = sum(h["classification"] == classification for h in enabled)
        if count != enabled_expected.get(classification, 0):
            fail(where + f"expected {enabled_expected.get(classification, 0)} enabled {classification} hooks")
    hook_source = "".join(p.read_text(encoding="utf-8") for p in sorted((rev_dir / "hooks").glob("*.cpp")))
    for h in enabled:
        if not re.search(rf"\b(void|bool)\s+{h['name']}\s*\(", hook_source):
            fail(where + f"enabled hook {h['name']} has no implementation in hooks/")
    if content["dlc"]["owner"] != "Xenon content/STFS subsystem": fail(where + "DLC ownership")
    if not patches["patches"]: fail(where + "patch classification")

    hints_v2 = load("analysis/hints.v2.json")
    if len(hints_v2["functions"]) != 372: fail(where + "hint-set function coverage")

    with (rev_dir / "analysis" / "legacy-symbols.csv").open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if len(rows) < 10000: fail(where + "legacy symbol map unexpectedly incomplete")
    if any(r["activation"] != "reference-only" for r in rows): fail(where + "legacy symbols must stay reference-only")
    return f"{directory}: {len(rows)} legacy function records, {hook_count} hooks ({len(enabled)} enabled)"

def main() -> int:
    manifest = json.loads((ROOT / "xenon-module.json").read_text(encoding="utf-8"))
    if manifest.get("id") != "org.nimauria.project-gracemeria": fail("module id")
    if "4E4D07D1" not in manifest.get("gameIds", []): fail("title id")
    # A prebuilt native extension would turn off the launcher's per-disc
    # automatic preparation from xenon-analysis/.
    if "nativeExtension" in manifest: fail("the module must not declare a prebuilt nativeExtension")
    identities = manifest.get("content", {}).get("identities", [])
    advertised = sorted(m for i in identities for m in i.get("mediaIds", []))
    if advertised != sorted(media for _, media, _, _, _ in REVISIONS):
        fail("advertised media IDs must be exactly the revisions with profiles")
    if manifest.get("regions") != [region for _, _, region, _, _ in REVISIONS]:
        fail("advertised regions must be exactly the revisions with profiles")
    tu = manifest.get("titleUpdates", {})
    if tu.get("owner") != "Project Xenon XEX Loader V2": fail("title-update ownership")
    if tu.get("supportedEffectiveProfiles") != []: fail("unverified TU profile advertised")
    dlc = manifest.get("dlc", [])
    if len(dlc) != 65: fail("installed manifest DLC catalogue coverage")
    dlc_ids = [item.get("id") for item in dlc]
    if len(set(dlc_ids)) != len(dlc_ids): fail("duplicate DLC identifiers")
    if any(item.get("contentIds") for item in dlc): fail("unverified STFS content IDs must not be invented")
    if any(item.get("identityStatus") != "pending-stfs-header-verification" for item in dlc):
        fail("DLC identity verification status")

    for directory, media, _, hook_count, enabled in REVISIONS:
        print("metadata ok: " + check_revision(manifest, directory, media, hook_count, enabled))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

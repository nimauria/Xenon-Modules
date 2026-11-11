#!/usr/bin/env python3
"""Builds a Gracemeria revision profile for another executable revision of
Ace Combat 6 from an existing one.

Xenon's hint-port aligns the two executables and translates the Analysis Hint
Schema V2 set; this script carries Gracemeria's own address-bearing metadata
(known symbols, native-replacement candidates, hook classifications, the
legacy symbol reference) through the same alignment (hint-port --map), and
writes the revision, content and patch records for the new revision.

  port_revision.py --source usa-retail-tu0 --target eur-retail-rev1
                   --map us-to-eu.map.json --hints-v2 eu-hints.v2.json
                   --media-id 0379EFB3 --region PAL --label "Europe Rev 1"

Addresses that do not translate are listed and left out (legacy symbols) or
kept disabled with "unmapped-in-revision" (hooks), never guessed.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AddressMap:
    def __init__(self, path: Path):
        data = json.loads(path.read_text(encoding="utf-8"))
        self.segments = sorted(data["segments"])
        self.sources = [s[0] for s in self.segments]
        self.source_hash = data["sourceImageHash"]
        self.target_hash = data["targetImageHash"]

    def translate(self, address: int) -> int | None:
        i = bisect.bisect_right(self.sources, address) - 1
        if i < 0:
            return None
        source, target, words = self.segments[i]
        offset = address - source
        return target + offset if 0 <= offset < words * 4 else None


def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def save(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", required=True, help="existing revision directory under revisions/")
    parser.add_argument("--target", required=True, help="revision directory to create")
    parser.add_argument("--map", required=True, type=Path, help="hint-port --map output")
    parser.add_argument("--hints-v2", required=True, type=Path, help="hint-port's translated hint set")
    parser.add_argument("--media-id", required=True)
    parser.add_argument("--region", required=True)
    parser.add_argument("--label", required=True, help="display label, e.g. 'Europe Rev 1'")
    args = parser.parse_args()

    source_dir = ROOT / "revisions" / args.source
    target_dir = ROOT / "revisions" / args.target
    amap = AddressMap(args.map)
    source_rev = load(source_dir / "revision.json")
    media = args.media_id.upper()
    revision_id = f"ac6-{args.target.split('-')[0]}-retail-{media.lower()}-{args.target.split('-')[-1]}"
    unmapped: list[str] = []

    def tr(address: int, what: str) -> int | None:
        mapped = amap.translate(address)
        if mapped is None:
            unmapped.append(f"{what} 0x{address:08X}")
        return mapped

    # Analysis Hint Schema V2: Xenon's translation, as written by hint-port.
    hints_v2 = load(args.hints_v2)
    save(target_dir / "analysis" / "hints.v2.json", hints_v2)

    hints = load(source_dir / "analysis" / "hints.json")
    hints["revision"] = revision_id
    symbols = []
    for symbol in hints["knownSymbols"]:
        mapped = tr(symbol["address"], f"known symbol {symbol['name']}")
        if mapped is not None:
            symbols.append({**symbol, "address": mapped})
    hints["knownSymbols"] = symbols
    sites = []
    for site in hints["indirectCallSites"]:
        mapped = tr(site["address"], "indirect call site")
        if mapped is not None:
            sites.append({**site, "address": mapped, "source": f"{site['source']} (translated from {args.source})"})
    hints["indirectCallSites"] = sites
    for name, address in list(hints.get("specialAddresses", {}).items()):
        hints["specialAddresses"][name] = tr(address, f"special address {name}")
    save(target_dir / "analysis" / "hints.json", hints)

    replacements = load(source_dir / "analysis" / "native-replacements.json")
    replacements["revision"] = revision_id
    ported = []
    for mapping in replacements["mappings"]:
        # Entries the alignment cannot place come from hint-port's own
        # reference-based translation of the same address.
        mapped = amap.translate(mapping["guestAddress"])
        if mapped is None:
            by_name = [r for r in hints_v2["nativeReplacements"] if r.get("sourceName") == mapping["name"]]
            mapped = by_name[0]["guestAddress"] if len(by_name) == 1 else None
        if mapped is None:
            unmapped.append(f"native replacement {mapping['name']} 0x{mapping['guestAddress']:08X}")
            continue
        ported.append({**mapping, "guestAddress": mapped})
    replacements["mappings"] = ported
    save(target_dir / "analysis" / "native-replacements.json", replacements)

    hooks = load(source_dir / "analysis" / "hooks.json")
    hooks["revision"] = revision_id
    for hook in hooks["hooks"]:
        mapped = amap.translate(hook["address"])
        if mapped is None:
            hook["sourceAddress"] = hook["address"]
            hook["classification"] = "unmapped-in-revision"
            hook["enabled"] = False
            hook["rationale"] = (f"The code at {args.source} 0x{hook['address']:08X} differs in this revision; "
                                 "the site has not been located here.")
            unmapped.append(f"hook {hook['name']} 0x{hook['address']:08X}")
            continue
        hook["address"] = mapped
        if hook.get("jumpAddressOnTrue"):
            hook["jumpAddressOnTrue"] = tr(hook["jumpAddressOnTrue"], f"hook {hook['name']} jump target")

    # Sites the alignment cannot carry (the code differs) are located by hand
    # and recorded in the target's port-overrides.json, so regenerating the
    # profile reproduces them.
    overrides_path = target_dir / "port-overrides.json"
    overrides = load(overrides_path) if overrides_path.exists() else {}
    if "baselinePolicy" in overrides:
        hooks["baselinePolicy"] = overrides["baselinePolicy"]
    by_name = {h["name"]: h for h in hooks["hooks"]}
    for override in overrides.get("hooks", []):
        hook = by_name.get(override["name"])
        if hook is None:
            raise SystemExit(f"port-overrides.json names unknown hook {override['name']}")
        hook.pop("sourceAddress", None)
        hook.update({k: v for k, v in override.items() if k != "name"})
        unmapped[:] = [u for u in unmapped if not u.startswith(f"hook {override['name']} ")]
    save(target_dir / "analysis" / "hooks.json", hooks)

    # Reference-only legacy names keep their original (source-revision) label.
    legacy_in = source_dir / "analysis" / "legacy-symbols.csv"
    legacy_out = target_dir / "analysis" / "legacy-symbols.csv"
    dropped = 0
    with legacy_in.open(newline="", encoding="utf-8") as fin, legacy_out.open("w", newline="", encoding="utf-8") as fout:
        reader = csv.DictReader(fin)
        writer = csv.DictWriter(fout, fieldnames=reader.fieldnames, lineterminator="\n")
        writer.writeheader()
        for row in reader:
            mapped = amap.translate(int(row["address"], 16))
            if mapped is None:
                dropped += 1
                continue
            writer.writerow({**row, "address": f"0x{mapped:08X}"})

    content = load(source_dir / "content" / "content.json")
    content["mediaId"] = media
    content["region"] = args.region
    content["titleUpdates"] = {
        "base": {"label": f"{args.label} executable", "analysisHints": "../analysis/hints.json",
                 "status": "translated-from-" + args.source},
        "knownUpdates": [],
    }
    save(target_dir / "content" / "content.json", content)

    patches = load(source_dir / "compatibility" / "patches.json")
    patches["revision"] = revision_id
    patch_overrides = {p["id"]: p for p in overrides.get("patches", [])}
    for patch in patches["patches"]:
        patch.update({k: v for k, v in patch_overrides.get(patch["id"], {}).items() if k != "id"})
    save(target_dir / "compatibility" / "patches.json", patches)

    revision = dict(source_rev)
    revision["id"] = revision_id
    revision["displayName"] = f"Ace Combat 6 {args.label} (Media ID {media})"
    revision["identity"] = {**source_rev["identity"], "mediaIds": [media], "effectiveImageHash": amap.target_hash}
    revision["status"] = "translated-from-" + args.source
    revision["translatedFrom"] = {"revision": args.source, "sourceImageHash": amap.source_hash,
                                  "tool": "Xenon hint-port alignment"}
    revision["notes"] = [
        f"Addresses are translated from {args.source} by Xenon's hint-port: the two executables are aligned "
        "instruction by instruction (relocation fields ignored) and each address is verified against the "
        "instruction found there; entries outside the alignment are placed by their verified references.",
        f"Media ID {media} only; another revision needs its own translated or analysed profile.",
    ]
    save(target_dir / "revision.json", revision)

    print(f"{args.target}: {len(symbols)} known symbols, {len(sites)} indirect call sites, "
          f"{len(ported)} native replacement candidates, {len(hooks['hooks'])} hook classifications, "
          f"{dropped} legacy symbols outside the alignment")
    for line in unmapped:
        print(f"  unmapped {line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

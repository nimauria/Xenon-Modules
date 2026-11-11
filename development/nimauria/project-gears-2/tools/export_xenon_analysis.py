#!/usr/bin/env python3
"""Writes the Xenon module hint package (xenon-analysis/) from revisions/.

xenon-prepare reads xenon-analysis/manifest.json and
revisions/<effective image hash>/analysis.json (Xenon Analysis Hint Schema
V2), plus the hook sources in revisions/<hash>/hooks/. This project keeps the
authored data in revisions/<revision>/ (revision.json, analysis/hints.json,
analysis/hooks.json, hooks/*.cpp); this tool converts it.

  export_xenon_analysis.py [--check]

--check exits 1 when xenon-analysis/ is not what the export would write.
"""
import json
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
REVISIONS = ROOT / "revisions"
PACKAGE = ROOT / "xenon-analysis"
MODULE_NAME = "Project Gears 2"
LIST_KEYS = ("chunks", "functions", "hooks", "indirectBranches", "indirectCalls",
             "instructionPatterns", "nativeReplacements", "patches", "regions",
             "runtimeHelpers", "switches", "symbols")


def hex_u32(value, where):
    if isinstance(value, int):
        number = value
    else:
        number = int(str(value), 16)
    if not 0 <= number <= 0xFFFFFFFF:
        raise ValueError(f"{where}: {value} is not a 32-bit value")
    return number


def load(path):
    with open(path, encoding="utf-8") as file:
        return json.load(file)


def build_revision(revision_dir):
    revision = load(revision_dir / "revision.json")
    identity = revision["identity"]
    hints = load(revision_dir / revision["analysisHints"])
    hooks = load(revision_dir / revision["hookClassification"])
    name = revision_dir.name

    functions = []
    for entry in hints.get("functions", []):
        where = f"{name} function {entry.get('address')}"
        out = {"address": hex_u32(entry["address"], where),
               "flags": sorted(entry.get("flags", []))}
        if entry.get("end") is not None:
            out["end"] = hex_u32(entry["end"], where)
        if entry.get("size") is not None:
            out["size"] = hex_u32(entry["size"], where)
        if entry.get("name"):
            out["name"] = entry["name"]
        functions.append(out)

    chunks = [{"start": hex_u32(c["start"], name), "end": hex_u32(c["end"], name),
               "parentFunction": hex_u32(c["parentFunction"], name)}
              for c in hints.get("chunks", [])]
    indirect_calls = [{"callsite": hex_u32(c["callsite"], name),
                       "targets": [hex_u32(t, name) for t in c.get("targets", [])]}
                      for c in hints.get("indirectCalls", [])]
    helpers = [{"address": hex_u32(h["address"], name), "kind": h["kind"]}
               for h in hints.get("runtimeHelpers", [])]
    replacements = [{"guestAddress": hex_u32(r["guestAddress"], name), "kind": r["kind"],
                     "sourceName": r.get("sourceName", "")}
                    for r in hints.get("nativeReplacements", [])]

    hook_entries = []
    for hook in hooks.get("hooks", []):
        if not hook.get("enabled", True):
            continue
        out = {"address": hex_u32(hook["address"], name),
               "nativeReplacementIdentity": hook["symbol"],
               "description": hook.get("description", ""),
               "kind": "instruction",
               "symbol": hook["symbol"]}
        if hook.get("afterInstruction"):
            out["afterInstruction"] = True
        for key, target in (("onTrue", "jumpAddressOnTrue"), ("onFalse", "jumpAddressOnFalse")):
            action = hook.get(key, "continue")
            if action not in ("continue", "return", "jump"):
                raise ValueError(f"{name} hook {hook['symbol']}: {key} '{action}'")
            if action != "continue":
                out[key] = action
            if action == "jump":
                out[target] = hex_u32(hook[target], name)
        hook_entries.append(out)

    analysis = {key: [] for key in LIST_KEYS}
    analysis.update({
        "schemaVersion": 2,
        "moduleName": name,
        "identity": {
            "titleId": hex_u32(identity["titleId"], name),
            "mediaId": hex_u32(identity["mediaId"], name),
            "baseVersion": hex_u32(identity["baseVersion"], name),
            "effectiveVersion": hex_u32(identity["effectiveVersion"], name),
            "titleUpdateApplied": bool(identity["titleUpdateApplied"]),
            "effectiveImageHash": identity["effectiveImageHash"].lower(),
        },
        "functions": sorted(functions, key=lambda f: f["address"]),
        "chunks": sorted(chunks, key=lambda c: c["start"]),
        "indirectCalls": sorted(indirect_calls, key=lambda c: c["callsite"]),
        "runtimeHelpers": sorted(helpers, key=lambda h: h["address"]),
        "nativeReplacements": sorted(replacements, key=lambda r: r["guestAddress"]),
        "hooks": sorted(hook_entries, key=lambda h: h["address"]),
    })
    sources = sorted((revision_dir / "hooks").glob("*.cpp")) if (revision_dir / "hooks").is_dir() else []
    return identity["effectiveImageHash"].lower(), analysis, sources


def expected_files():
    files = {}
    hashes = []
    for revision_dir in sorted(p for p in REVISIONS.iterdir() if (p / "revision.json").is_file()):
        image_hash, analysis, sources = build_revision(revision_dir)
        hashes.append(image_hash)
        base = pathlib.PurePosixPath("revisions") / image_hash
        files[str(base / "analysis.json")] = json.dumps(analysis, indent=2, sort_keys=True) + "\n"
        for source in sources:
            files[str(base / "hooks" / source.name)] = source.read_text(encoding="utf-8")
    manifest = {"compatibilityVersion": "1", "moduleName": MODULE_NAME, "revisions": hashes}
    files["manifest.json"] = json.dumps(manifest, indent=2) + "\n"
    return files


def existing_files():
    if not PACKAGE.is_dir():
        return {}
    return {p.relative_to(PACKAGE).as_posix(): p.read_text(encoding="utf-8")
            for p in PACKAGE.rglob("*") if p.is_file()}


def main():
    check = "--check" in sys.argv[1:]
    expected = expected_files()
    if check:
        actual = existing_files()
        stale = sorted(set(expected) ^ set(actual) |
                       {k for k in expected.keys() & actual.keys() if expected[k] != actual[k]})
        if stale:
            print("xenon-analysis/ is stale (run tools/export_xenon_analysis.py):")
            for path in stale:
                print("  " + path)
            return 1
        print(f"xenon-analysis/ is current ({len(expected)} files)")
        return 0
    if PACKAGE.exists():
        shutil.rmtree(PACKAGE)
    for relative, text in expected.items():
        path = PACKAGE / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {len(expected)} files to {PACKAGE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

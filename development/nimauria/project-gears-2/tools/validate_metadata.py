#!/usr/bin/env python3
"""Checks Project Gears 2's metadata for consistency.

- xenon-module.json lists every revision's title and media ID;
- each revision's identity is well formed (8-hex IDs and versions, 40-hex
  effective image hash) and unique;
- hint and hook addresses are word-aligned guest code addresses
  (0x82000000-0x8FFFFFFF), unique per revision, with sane function bounds;
- every hook names a symbol that one of the revision's hooks/*.cpp defines.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HEX8 = re.compile(r"^[0-9A-Fa-f]{8}$")
HEX40 = re.compile(r"^[0-9a-f]{40}$")
errors = []


def fail(message):
    errors.append(message)


def load(path):
    try:
        with open(path, encoding="utf-8") as file:
            return json.load(file)
    except (OSError, ValueError) as error:
        fail(f"{path}: {error}")
        return {}


def address(value, where):
    try:
        number = int(str(value), 16)
    except ValueError:
        fail(f"{where}: '{value}' is not hex")
        return None
    if number % 4 or not 0x82000000 <= number <= 0x8FFFFFFF:
        fail(f"{where}: {value} is not a word-aligned guest code address")
    return number


def main():
    module = load(ROOT / "xenon-module.json")
    if module.get("schema") != "xenon.game-module":
        fail("xenon-module.json: schema must be xenon.game-module")
    listed = {(i["titleId"].upper(), m.upper())
              for i in module.get("content", {}).get("identities", []) for m in i.get("mediaIds", [])}

    hashes = set()
    revisions = sorted(p for p in (ROOT / "revisions").iterdir() if (p / "revision.json").is_file())
    if not revisions:
        fail("revisions/: no revision")
    for revision_dir in revisions:
        name = revision_dir.name
        revision = load(revision_dir / "revision.json")
        identity = revision.get("identity", {})
        for key in ("titleId", "mediaId", "baseVersion", "effectiveVersion"):
            if not HEX8.match(str(identity.get(key, ""))):
                fail(f"{name}: identity.{key} must be 8 hex digits")
        image_hash = str(identity.get("effectiveImageHash", ""))
        if not HEX40.match(image_hash):
            fail(f"{name}: identity.effectiveImageHash must be 40 lowercase hex digits")
        if image_hash in hashes:
            fail(f"{name}: effectiveImageHash {image_hash} is used by another revision")
        hashes.add(image_hash)
        if (str(identity.get("titleId", "")).upper(), str(identity.get("mediaId", "")).upper()) not in listed:
            fail(f"{name}: title/media ID is not listed in xenon-module.json content.identities")

        hints = load(revision_dir / revision.get("analysisHints", "analysis/hints.json"))
        seen = set()
        for entry in hints.get("functions", []):
            where = f"{name} function {entry.get('address')}"
            start = address(entry.get("address"), where)
            if start in seen:
                fail(f"{where}: duplicate")
            seen.add(start)
            if entry.get("end") is not None:
                end = address(entry["end"], where + " end")
                if start is not None and end is not None and end <= start:
                    fail(f"{where}: end is not after the start")
            if not entry.get("source"):
                fail(f"{where}: no source recorded")
            for flag in entry.get("flags", []):
                if flag not in ("NoReturn", "Leaf", "Thunk"):
                    fail(f"{where}: unknown flag {flag}")

        hooks = load(revision_dir / revision.get("hookClassification", "analysis/hooks.json"))
        sources = "".join(p.read_text(encoding="utf-8")
                          for p in (revision_dir / "hooks").glob("*.cpp")) if (revision_dir / "hooks").is_dir() else ""
        hook_addresses = set()
        for hook in hooks.get("hooks", []):
            symbol = hook.get("symbol", "")
            where = f"{name} hook {symbol or hook.get('address')}"
            at = address(hook.get("address"), where)
            if at in hook_addresses:
                fail(f"{where}: two hooks at one address")
            hook_addresses.add(at)
            if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", symbol):
                fail(f"{where}: symbol must be a C++ identifier")
            elif not re.search(r"\b(?:bool|void)\s+" + symbol + r"\s*\(", sources):
                fail(f"{where}: no hooks/*.cpp defines {symbol}")
            if not hook.get("source"):
                fail(f"{where}: no source recorded")

    for message in errors:
        print("error: " + message)
    if errors:
        return 1
    print(f"metadata ok ({len(revisions)} revision(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Stages the installable Project Gracemeria module package.

The package carries no native code: the launcher's automatic preparation
builds the native module from the player's own disc using xenon-analysis/
(one revision profile per supported disc), so one package serves every
supported revision.
"""
from __future__ import annotations
import argparse, json, shutil, sys
from pathlib import Path

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output", type=Path, default=Path("dist/org.nimauria.project-gracemeria"))
    args=ap.parse_args()
    root=Path(__file__).resolve().parents[1]
    analysis=root/"xenon-analysis"
    if not (analysis/"manifest.json").exists():
        raise SystemExit("xenon-analysis/ is missing - build the gracemeria-xenon-analysis target first")
    manifest=json.loads((root/"xenon-module.json").read_text(encoding="utf-8"))
    if "nativeExtension" in manifest:
        raise SystemExit("xenon-module.json declares a prebuilt nativeExtension, which disables automatic preparation")
    out=args.output
    if out.exists(): shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(root/"xenon-module.json", out/"xenon-module.json")
    shutil.copy2(root/"README.md", out/"README.md")
    shutil.copy2(root/"LICENSE", out/"LICENSE")
    shutil.copytree(root/"revisions", out/"revisions")
    shutil.copytree(analysis, out/"xenon-analysis")
    print(out)
    return 0
if __name__=="__main__": raise SystemExit(main())

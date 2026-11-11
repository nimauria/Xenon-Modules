#!/usr/bin/env python3
"""Checks that gracemeria-export-analysis ships a revision's
rendering/rendering.json into xenon-analysis/ and refuses a malformed one.

  check_rendering_export.py <gracemeria-export-analysis executable>
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVISION = "usa-retail-tu0"
HASH = "464905ef5d5a80d8b52697f72f16e8c93c4b81ba"
VALID = '{"schema":"xenon.rendering.v1","temporal":{"camera":{"vertexConstant":0,"layout":"rows"}}}\n'
INVALID = '{"schema":"xenon.rendering.v1","temporal":{"camera":{"layout":"rows"}}}\n'


def export(exe: str, revisions: Path, out: Path) -> subprocess.CompletedProcess:
    return subprocess.run([exe, str(out), "--revisions", str(revisions)], capture_output=True, text=True)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    with tempfile.TemporaryDirectory() as temp:
        revisions = Path(temp) / "revisions"
        shutil.copytree(ROOT / "revisions", revisions)
        rendering = revisions / REVISION / "rendering" / "rendering.json"
        rendering.parent.mkdir(parents=True, exist_ok=True)

        rendering.write_text(VALID, encoding="utf-8", newline="")
        result = export(sys.argv[1], revisions, Path(temp) / "good")
        shipped = Path(temp) / "good" / "revisions" / HASH / "rendering.json"
        if result.returncode != 0 or not shipped.exists() or shipped.read_text(encoding="utf-8") != VALID:
            print("a valid rendering profile was not shipped:", result.stdout, result.stderr)
            return 1
        others = [p for p in (Path(temp) / "good" / "revisions").glob("*/rendering.json") if p != shipped]
        if others:
            print("a rendering profile reached another revision:", others)
            return 1

        rendering.write_text(INVALID, encoding="utf-8", newline="")
        result = export(sys.argv[1], revisions, Path(temp) / "bad")
        if result.returncode == 0 or "vertexConstant is required" not in result.stderr:
            print("a malformed rendering profile was not refused:", result.returncode, result.stderr)
            return 1
    print("rendering profiles: valid shipped for its revision only, malformed refused")
    return 0


if __name__ == "__main__":
    sys.exit(main())

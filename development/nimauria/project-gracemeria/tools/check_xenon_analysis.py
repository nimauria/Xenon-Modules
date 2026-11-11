#!/usr/bin/env python3
"""Fails when xenon-analysis/ (the Xenon module hint package the launcher
prepares from) is out of date with revisions/. Regenerate it with the
gracemeria-xenon-analysis build target.

  check_xenon_analysis.py <gracemeria-export-analysis executable>
"""
from __future__ import annotations

import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def files(root: Path) -> set[str]:
    return {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    committed = ROOT / "xenon-analysis"
    with tempfile.TemporaryDirectory() as temp:
        fresh = Path(temp) / "xenon-analysis"
        subprocess.run([sys.argv[1], str(fresh), "--revisions", str(ROOT / "revisions")], check=True,
                       stdout=subprocess.DEVNULL)
        expected, actual = files(fresh), files(committed) if committed.exists() else set()
        stale = sorted(expected ^ actual)
        stale += sorted(f for f in expected & actual if not filecmp.cmp(fresh / f, committed / f, shallow=False))
    if stale:
        print("xenon-analysis/ is out of date with revisions/ (build gracemeria-xenon-analysis):")
        for name in stale:
            print("  " + name)
        return 1
    print(f"xenon-analysis/ is current ({len(expected)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

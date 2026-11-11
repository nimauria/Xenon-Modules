#!/usr/bin/env python3
"""Reduce AC6_recomp's legacy 10k+ function table after real Xenon analysis.

Input is Xenon's analysis.json from a legally supplied supported AC6 XEX.
Only legacy starts absent from Xenon's discovered function list are emitted.
This deliberately keeps the shipped active hint set small.
"""
from __future__ import annotations
import argparse, csv, json
from pathlib import Path

def parse_addr(text: str) -> int:
    return int(text, 0)

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("analysis_json", type=Path)
    ap.add_argument("--symbols", type=Path, default=Path(__file__).resolve().parents[1] / "revisions/usa-retail-tu0/analysis/legacy-symbols.csv")
    ap.add_argument("--output", type=Path, default=Path("retained-function-hints.json"))
    args=ap.parse_args()
    report=json.loads(args.analysis_json.read_text(encoding="utf-8"))
    discovered={int(f["start"]) for f in report.get("functions",[])}
    missing=[]
    with args.symbols.open(newline="",encoding="utf-8") as f:
        for row in csv.DictReader(f):
            addr=parse_addr(row["address"])
            if addr not in discovered:
                missing.append({"address":addr,"name":row["name"]})
    out={"schema":"project-gracemeria.retained-function-hints","version":1,
         "source":str(args.analysis_json),"missingFromAutomaticAnalysis":missing}
    args.output.write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8")
    print(f"automatic analysis discovered {len(discovered)} functions; retained {len(missing)} legacy starts")
    return 0
if __name__=="__main__": raise SystemExit(main())

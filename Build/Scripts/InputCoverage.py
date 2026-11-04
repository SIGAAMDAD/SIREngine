#!/usr/bin/env python3
"""Strict LLVM source-coverage gate for SIREngine native input TUs."""
import argparse
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--export", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = args.source.resolve()
    prefix = root / "Source" / "Core" / "Platform" / "Input"
    compdb = json.loads(args.compdb.read_text())
    compiled = set()
    for command in compdb:
        path = Path(command["file"])
        if not path.is_absolute():
            path = Path(command["directory"]) / path
        path = path.resolve()
        if path.suffix == ".cpp" and (prefix == path.parent or prefix in path.parents):
            compiled.add(str(path))
    if not compiled:
        print("COVERAGE FAIL: no production input compilation units in authentic compilation database")
        return 3
    export = json.loads(args.export.read_text())
    measured = {}
    for entry in export.get("data", []):
        for file in entry.get("files", []):
            path = str(Path(file["filename"]).resolve())
            if path not in compiled:
                continue
            if path in measured:
                print(f"COVERAGE FAIL: duplicate file coverage mapping {path}")
                return 3
            measured[path] = file["summary"]
    counts = ["lines", "branches", "functions", "regions"]
    totals = {name: {"count": 0, "covered": 0} for name in counts}
    records = []
    missing = sorted(compiled - set(measured))
    for path in sorted(compiled):
        summary = measured.get(path)
        row = {"file": str(Path(path).relative_to(root)), "status": "MISSING_MAPPING" if summary is None else "MEASURED"}
        if summary is not None:
            for name in counts:
                stat = summary.get(name, {})
                count = int(stat.get("count", 0))
                covered = int(stat.get("covered", 0))
                if not 0 <= covered <= count:
                    raise ValueError(f"invalid LLVM counters for {path} {name}")
                row[name] = {"covered": covered, "count": count, "percent": round(100 * covered / count, 4) if count else None}
                totals[name]["count"] += count
                totals[name]["covered"] += covered
        records.append(row)
    result = {
        "source_root": str(root),
        "compiled_input_translation_units": len(compiled),
        "missing_mappings": [str(Path(p).relative_to(root)) for p in missing],
        "files": records,
        "totals": totals,
        "coverage_pass": not missing and all(
            totals[name]["count"] > 0 and totals[name]["covered"] == totals[name]["count"] for name in counts
        ) and all(
            all(row[name]["covered"] == row[name]["count"] for name in counts if row[name]["count"])
            for row in records if row["status"] == "MEASURED"
        ),
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, indent=2) + "\n")
    for row in records:
        line = " ".join(
            f'{name}={row[name]["covered"]}/{row[name]["count"]}' for name in counts
        ) if row["status"] == "MEASURED" else "MISSING"
        print(row["file"], line)
    print("Total:", " ".join(f'{k}={v["covered"]}/{v["count"]}' for k, v in totals.items()))
    print("Input coverage status:", "PASS" if result["coverage_pass"] else "INCOMPLETE")
    return 0 if result["coverage_pass"] else 3


if __name__ == "__main__":
    sys.exit(main())

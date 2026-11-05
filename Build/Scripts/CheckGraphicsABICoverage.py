#!/usr/bin/env python3
"""
Source-based, fail-closed LLVM coverage gate for the complete shared ABI
implementation. No skipped .cpp files or zero-denominator PASS results.
"""
import json
import os
import pathlib
import sys

SOURCE_FILES = (
    "Source/Renderer/SIRGraphicsDriverABI.cpp",
    "Source/Renderer/SIRGraphicsModule.cpp",
    "Source/Renderer/SIRGraphicsPipelineABI.cpp",
    "Source/Renderer/SIRGraphicsPipelineBridge.cpp",
    "Source/Renderer/SIRGraphicsRegistry.cpp",
    "Source/Renderer/SIRGraphicsValidation.cpp",
)
METRICS = ("lines", "branches", "functions", "regions")


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: CheckGraphicsABICoverage.py llvm-export.json repo-root report.json")
    source = pathlib.Path(sys.argv[1])
    repo = pathlib.Path(sys.argv[2]).resolve()
    destination = pathlib.Path(sys.argv[3])
    data = json.loads(source.read_text(encoding="utf-8"))
    minimum = int(os.environ.get("SIR_GRAPHICS_ABI_MIN_COVERAGE", "90"))
    if minimum < 0 or minimum > 100:
        raise SystemExit("coverage threshold must be 0..100")
    measured = {}
    for entry in data.get("data", []):
        for file in entry.get("files", []):
            name = pathlib.Path(file["filename"]).resolve()
            try:
                rel = name.relative_to(repo).as_posix()
            except ValueError:
                continue
            if rel not in SOURCE_FILES:
                continue
            if rel in measured:
                raise SystemExit("duplicate source mapping: " + rel)
            measured[rel] = file.get("summary", {})

    missing = sorted(set(SOURCE_FILES) - set(measured))
    totals = {key: {"covered": 0, "count": 0, "percent": 0.0} for key in METRICS}
    errors = []
    if missing:
        errors.append("missing compiled source mappings: " + ", ".join(missing))
    for name in SOURCE_FILES:
        if name not in measured:
            continue
        for metric in METRICS:
            record = measured[name].get(metric, {})
            count = record.get("count", 0)
            covered = record.get("covered", 0)
            if not isinstance(count, int) or not isinstance(covered, int) or count < 0 or covered < 0 or covered > count:
                errors.append(name + ": corrupt " + metric + " counters")
                continue
            totals[metric]["count"] += count
            totals[metric]["covered"] += covered
    for metric in METRICS:
        item = totals[metric]
        if item["count"] == 0:
            errors.append(metric + ": empty denominator")
            continue
        item["percent"] = round(100.0 * item["covered"] / item["count"], 2)
        if item["percent"] < minimum:
            errors.append(metric + ": " + str(item["percent"]) + "% below " + str(minimum) + "%")
    result = {
        "system": "GraphicsABI",
        "source_scope": list(SOURCE_FILES),
        "minimum_percent": minimum,
        "metrics": totals,
        "missing_sources": missing,
        "status": "PASS" if not errors else "FAIL",
        "errors": errors,
    }
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if not errors else 3


if __name__ == "__main__":
    raise SystemExit(main())

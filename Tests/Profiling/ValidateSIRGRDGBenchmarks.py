#!/usr/bin/env python3
"""Validate required GRDG and platform-I/O Google Benchmark scenarios."""
import argparse
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Build" / "CI"))
from CompareBenchmarks import median_records

SCENARIOS = {
    "SIRBenchmarkGRDGNativeReadAt": ("4096", "65536", "1048576"),
    "SIRBenchmarkGRDGMappedCopy": ("4096", "65536", "1048576"),
    "SIRBenchmarkGRDGLookup": ("32", "256", "1024"),
    "SIRBenchmarkGRDGPublishReplace": ("64", "4096", "65536"),
    "SIRBenchmarkGRDGFileIngest": (
        "4096/0", "4096/1", "65536/0", "65536/1", "1048576/0", "1048576/1"
    ),
    "SIRBenchmarkGRDGPageRetireCancel": ("1", "16"),
    "SIRBenchmarkGRDGPageDrainRecommit": ("1", "16"),
    "SIRBenchmarkPlatformReadAt": ("64/0", "4096/0", "4096/37", "65536/37", "1048576/37"),
    "SIRBenchmarkPlatformWriteAt": ("64", "4096", "65536"),
    "SIRBenchmarkPlatformSequentialReadSeek": ("4096", "65536"),
    "SIRBenchmarkPlatformOpenClose": ("",),
    "SIRBenchmarkPlatformMapAndCopy": ("4096", "65536", "1048576"),
    "SIRBenchmarkPlatformMappedReuseCopy": ("4096", "65536", "1048576"),
    "SIRBenchmarkPlatformFileInfo": ("",),
    "SIRBenchmarkPlatformFileSystemQuery": ("",),
    "SIRBenchmarkPlatformSemaphoreUncontended": ("",),
    "SIRBenchmarkPlatformSemaphoreHandoff": ("",),
    "SIRBenchmarkPlatformPipeLifecycleTransfer": ("4096", "65536"),
}
REQUIRED = {f"{family}/{case}" if case else family for family, cases in SCENARIOS.items() for case in cases}


def read_measurements(path):
    with Path(path).open(encoding="utf-8") as stream:
        doc = json.load(stream)
    records = median_records(doc)
    found = {}
    for _, (real_ns, item) in records.items():
        name = item.get("name", "")
        if not isinstance(name, str) or not name.endswith("_median"):
            continue
        case = name[:-len("_median")]
        if case not in REQUIRED:
            continue
        cpu = item.get("cpu_time")
        if not isinstance(cpu, (int, float)) or not math.isfinite(cpu) or cpu < 0:
            raise ValueError(f"{path}: invalid CPU time for {case}: {cpu}")
        if case in found:
            raise ValueError(f"{path}: duplicate median for {case}")
        found[case] = (real_ns, item)
    missing = sorted(REQUIRED - found.keys())
    if missing:
        raise ValueError(f"{path}: {len(missing)} missing benchmark medians: {', '.join(missing)}")
    return doc.get("context", {}), found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate", help="Google Benchmark JSON from the current commit")
    args = parser.parse_args()
    try:
        _, candidate = read_measurements(args.candidate)
        print(f"GRDG/platform I/O benchmark coverage PASS: {len(candidate)} required median scenarios")
        for name, (ns, _) in sorted(candidate.items()):
            print(f"  {name}: {ns:,.2f} ns median")
        return 0
    except (ValueError, KeyError, OSError, RuntimeError, json.JSONDecodeError) as error:
        print(f"GRDG benchmark validation FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

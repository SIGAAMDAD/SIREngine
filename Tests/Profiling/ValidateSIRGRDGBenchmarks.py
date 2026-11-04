#!/usr/bin/env python3
"""Validate GRDG benchmark coverage and optionally compare matched CPU runs.

No universal latency threshold is asserted. The baseline must be a prior JSON
from the same benchmark configuration, machine class, and build type.
"""
import argparse
import json
import math
import sys
from pathlib import Path

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
}
REQUIRED = {f"{family}/{case}" for family, cases in SCENARIOS.items() for case in cases}


def read_measurements(path):
    with Path(path).open(encoding="utf-8") as stream:
        doc = json.load(stream)
    runs = doc.get("benchmarks")
    if not isinstance(runs, list):
        raise ValueError(f"{path}: Google Benchmark 'benchmarks' array missing")
    found = {}
    for item in runs:
        name = item.get("name", "")
        if not isinstance(name, str) or not name.endswith("_median"):
            continue
        base = name[:-len("_median")]
        if base not in REQUIRED:
            continue
        if item.get("error_occurred"):
            raise ValueError(f"{path}: benchmark error in {base}")
        real = item.get("real_time")
        cpu = item.get("cpu_time")
        unit = item.get("time_unit")
        if not isinstance(real, (int, float)) or not math.isfinite(real) or real <= 0:
            raise ValueError(f"{path}: invalid elapsed time for {base}: {real}")
        if not isinstance(cpu, (int, float)) or not math.isfinite(cpu) or cpu < 0:
            raise ValueError(f"{path}: invalid CPU time for {base}: {cpu}")
        if unit not in ("ns", "us", "ms", "s"):
            raise ValueError(f"{path}: invalid time unit for {base}: {unit}")
        if base in found:
            raise ValueError(f"{path}: duplicate median for {base}")
        found[base] = (float(real) * {"ns": 1, "us": 1e3, "ms": 1e6, "s": 1e9}[unit], item)
    missing = sorted(REQUIRED - found.keys())
    if missing:
        raise ValueError(f"{path}: {len(missing)} missing benchmark medians: {', '.join(missing)}")
    return doc.get("context", {}), found


def comparable_context(a, b):
    fields = ("num_cpus", "mhz_per_cpu", "library_build_type")
    mismatch = [f for f in fields if a.get(f) != b.get(f)]
    if a.get("host_name") != b.get("host_name"):
        mismatch.append("host_name")
    return mismatch


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate", help="Google Benchmark JSON from the current commit")
    parser.add_argument("--baseline", help="previously collected JSON for the same machine and configuration")
    parser.add_argument("--max-regression-percent", type=float, default=20.0)
    args = parser.parse_args()
    if not math.isfinite(args.max_regression_percent) or args.max_regression_percent < 0:
        parser.error("--max-regression-percent must be finite and non-negative")
    try:
        context, candidate = read_measurements(args.candidate)
        print(f"GRDG benchmark coverage PASS: {len(candidate)} required median scenarios")
        for name, (ns, _) in sorted(candidate.items()):
            print(f"  {name}: {ns:,.2f} ns median")
        if not args.baseline:
            print("No performance-regression verdict: no matched baseline supplied")
            return 0
        old_context, baseline = read_measurements(args.baseline)
        differences = comparable_context(context, old_context)
        if differences:
            raise ValueError("benchmark environments differ: " + ", ".join(differences))
        regressions = []
        for name, (new_ns, _) in sorted(candidate.items()):
            old_ns = baseline[name][0]
            pct = 100.0 * (new_ns / old_ns - 1.0)
            print(f"  COMPARE {name}: {pct:+.1f}% time vs matched baseline")
            if pct > args.max_regression_percent:
                regressions.append((name, pct))
        if regressions:
            print("Regression gate FAIL: " + ", ".join(f"{n} ({v:+.1f}%)" for n, v in regressions),
                  file=sys.stderr)
            return 1
        print(f"Regression gate PASS: no median exceeded +{args.max_regression_percent:.1f}%")
        return 0
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        print(f"GRDG benchmark validation FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Compare identical SIREngine Google Benchmark workloads using run medians.

Examples:
  python3 Tools/Profiling/SIRCompareBenchmarks.py compare baseline.json candidate.json --output results.json
  python3 Tools/Profiling/SIRCompareBenchmarks.py rank profile.json --family SIRBenchmarkExperimentReplay --output policies.json
"""
import argparse
import json
import math
import pathlib
import sys

UNITS_NS = {"ns": 1.0, "us": 1_000.0, "ms": 1_000_000.0, "s": 1_000_000_000.0}
CONTEXT_FIELDS = ("host_name", "num_cpus", "mhz_per_cpu", "cpu_scaling_enabled")


def load(path):
    with open(path, "r", encoding="utf-8") as stream:
        data = json.load(stream)
    if not isinstance(data, dict) or not isinstance(data.get("benchmarks"), list):
        raise ValueError(f"{path}: missing benchmarks array")
    return data


def medians(data, metric="real_time"):
    result = {}
    for entry in data["benchmarks"]:
        name = entry.get("run_name", entry.get("name", ""))
        if entry.get("aggregate_name") != "median":
            continue
        if entry.get("error_occurred") or not name:
            continue
        value = entry.get(metric)
        unit = entry.get("time_unit", "ns")
        if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0 or unit not in UNITS_NS:
            raise ValueError(f"Invalid {metric} or unit: {name}")
        if name in result:
            raise ValueError(f"Duplicate median record: {name}")
        result[name] = value * UNITS_NS[unit]
    if not result:
        raise ValueError("No valid median aggregate records; use --benchmark_repetitions=5 and aggregate output")
    return result


def verify_context(a, b, allow_mismatch=False):
    context_a = a.get("context", {})
    context_b = b.get("context", {})
    mismatches = {}
    for name in CONTEXT_FIELDS:
        if name in context_a and name in context_b and context_a[name] != context_b[name]:
            mismatches[name] = [context_a[name], context_b[name]]
    if mismatches and not allow_mismatch:
        raise ValueError(f"Hardware/runtime mismatch; use matched runs or explicit --allow-context-mismatch: {mismatches}")
    return mismatches


def compare(a, b, metric="real_time", allow_mismatch=False):
    mismatches = verify_context(a, b, allow_mismatch)
    prior = medians(a, metric)
    after = medians(b, metric)
    common = sorted(set(prior) & set(after))
    if not common:
        raise ValueError("No matched median workload names")
    rows = []
    for name in common:
        old = prior[name]
        new = after[name]
        rows.append({
            "name": name,
            "baseline_ns": old,
            "candidate_ns": new,
            "delta_ns": new - old,
            "delta_pct": (new / old - 1.0) * 100.0,
            "speedup": old / new,
        })
    return {
        "metric": metric,
        "unit": "ns",
        "matched_count": len(rows),
        "baseline_only": sorted(set(prior) - set(after)),
        "candidate_only": sorted(set(after) - set(prior)),
        "context_mismatches": mismatches,
        "results": rows,
        "warning": "Magnitude-only comparison; medians are not statistical confidence intervals",
    }


def rank(data, family, metric="real_time"):
    """Rank matching candidates per profile ID with identical synthetic work.

    Expected name: SIRBenchmarkExperimentReplay/<profile>/<workers>/<chunk>.
    Never compare different profiles, different inputs or different dataset sizes.
    """
    entries = medians(data, metric)
    groups = {}
    prefix = family + "/"
    for name, duration in entries.items():
        if not name.startswith(prefix):
            continue
        tail = name[len(prefix):].split("/")
        if len(tail) != 3 or not all(part.isdecimal() for part in tail):
            continue
        profile, workers, chunk = (int(part) for part in tail)
        groups.setdefault(profile, []).append({"name": name, "workers": workers, "chunk": chunk, "median_ns": duration})
    if not groups:
        raise ValueError(f"No matching replay medians for family {family}")
    return {
        "family": family,
        "metric": metric,
        "unit": "ns",
        "rankings": {str(profile): sorted(candidates, key=lambda r: (r["median_ns"], r["workers"], r["chunk"]))
                     for profile, candidates in sorted(groups.items())},
        "warning": "Synthetic rankings are exploratory; native subsystem traces are required for policy adoption",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    p_compare = sub.add_parser("compare")
    p_compare.add_argument("baseline", type=pathlib.Path)
    p_compare.add_argument("candidate", type=pathlib.Path)
    p_compare.add_argument("--allow-context-mismatch", action="store_true")
    p_rank = sub.add_parser("rank")
    p_rank.add_argument("input", type=pathlib.Path)
    p_rank.add_argument("--family", default="SIRBenchmarkExperimentReplay")
    for cmd in (p_compare, p_rank):
        cmd.add_argument("--metric", choices=("real_time", "cpu_time"), default="real_time")
        cmd.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        if args.operation == "compare":
            result = compare(load(args.baseline), load(args.candidate), args.metric, args.allow_context_mismatch)
        else:
            result = rank(load(args.input), args.family, args.metric)
        serialized = json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(serialized, encoding="utf-8")
        else:
            sys.stdout.write(serialized)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    sys.exit(main())

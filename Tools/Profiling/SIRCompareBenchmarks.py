#!/usr/bin/env python3
"""Rank controlled SIREngine Google Benchmark experiments using validated medians.

Examples:
  python3 Build/CI/CompareBenchmarks.py compare --baseline baseline.json --candidate candidate.json
  python3 Tools/Profiling/SIRCompareBenchmarks.py rank profile.json --family SIRBenchmarkExperimentReplay --output policies.json
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "Build" / "CI"))
from CompareBenchmarks import median_samples


def load(path):
    with open(path, "r", encoding="utf-8") as stream:
        data = json.load(stream)
    if not isinstance(data, dict) or not isinstance(data.get("benchmarks"), list):
        raise ValueError(f"{path}: missing benchmarks array")
    return data


def rank(data, family, metric="real_time"):
    """Rank matching candidates per profile ID with identical synthetic work.

    Expected name: SIRBenchmarkExperimentReplay/<profile>/<workers>/<chunk>.
    Never compare different profiles, different inputs or different dataset sizes.
    """
    entries = median_samples(data, metric)
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
    parser.add_argument("operation", choices=("rank",))
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("--family", default="SIRBenchmarkExperimentReplay")
    parser.add_argument("--metric", choices=("real_time", "cpu_time"), default="real_time")
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        result = rank(load(args.input), args.family, args.metric)
        serialized = json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(serialized, encoding="utf-8")
        else:
            sys.stdout.write(serialized)
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    sys.exit(main())

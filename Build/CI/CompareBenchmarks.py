#!/usr/bin/env python3
"""Stamp benchmark artifacts and compare only identical execution environments."""
import argparse
import hashlib
import json
import pathlib
import platform
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cmake_values(path):
    result = {}
    for line in path.read_text().splitlines():
        if "=" in line and ":" in line.split("=", 1)[0] and not line.startswith("//"):
            name, value = line.split("=", 1)
            result[name.split(":", 1)[0]] = value
    return result


def stamp(data, cache, executable, source_sha):
    compiler = cache.get("CMAKE_CXX_COMPILER", "")
    if not compiler:
        raise RuntimeError("Missing configured compiler")
    version = subprocess.check_output([compiler, "--version"], text=True).splitlines()[0]
    data.setdefault("context", {})["sir_identity"] = {
        "host": platform.node(),
        "system": platform.system(),
        "machine": platform.machine(),
        "compiler": compiler,
        "compiler_version": version,
        "build_type": cache.get("CMAKE_BUILD_TYPE", ""),
        "flags": cache.get("CMAKE_CXX_FLAGS", ""),
        "presets_digest": digest(ROOT / "CMakePresets.json"),
        "source_sha": source_sha,
        "executable_sha256": digest(executable),
    }
    return data


def median_samples(data):
    samples = {}
    for case in data.get("benchmarks", []):
        if case.get("aggregate_name") != "median":
            continue
        name = case.get("name")
        unit = case.get("time_unit")
        value = case.get("real_time")
        if not name or unit not in ("ns", "us", "ms", "s") or not isinstance(value, (int, float)) or value <= 0:
            raise RuntimeError("Invalid benchmark median record")
        if name in samples:
            raise RuntimeError(f"Duplicate benchmark median: {name}")
        samples[name] = value * {"ns": 1, "us": 1000, "ms": 1000000, "s": 1000000000}[unit]
    if not samples:
        raise RuntimeError("No benchmark medians found")
    return samples


def compare(baseline, current, threshold=0.15):
    identity_keys = ("host", "system", "machine", "compiler", "compiler_version",
                     "build_type", "flags", "presets_digest")
    old = baseline.get("context", {}).get("sir_identity", {})
    new = current.get("context", {}).get("sir_identity", {})
    for key in identity_keys:
        if key not in old or key not in new or old[key] != new[key]:
            raise RuntimeError(f"Unmatched benchmark execution identity: {key}")
    if (not baseline.get("context", {}).get("cpu_info") or
            baseline.get("context", {}).get("cpu_info") != current.get("context", {}).get("cpu_info")):
        raise RuntimeError("Unmatched physical CPU configuration")
    old_samples, new_samples = median_samples(baseline), median_samples(current)
    if old_samples.keys() != new_samples.keys():
        raise RuntimeError("Benchmark workload identities differ")
    regressions = {
        name: new_samples[name] / old_samples[name]
        for name in old_samples if new_samples[name] > old_samples[name] * (1 + threshold)
    }
    if regressions:
        raise RuntimeError(f"Runtime benchmark regression threshold exceeded: {regressions}")
    return len(old_samples)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("stamp", "compare"))
    parser.add_argument("--candidate", required=True, type=pathlib.Path)
    parser.add_argument("--baseline", type=pathlib.Path)
    parser.add_argument("--build-dir", type=pathlib.Path)
    parser.add_argument("--binary", type=pathlib.Path)
    parser.add_argument("--max-slowdown", type=float, default=0.15)
    args = parser.parse_args(argv)
    if args.mode == "stamp":
        if not args.build_dir or not args.binary:
            raise RuntimeError("Stamps require the executable and CMake build directory")
        source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        data = stamp(json.loads(args.candidate.read_text()),
                     cmake_values(args.build_dir / "CMakeCache.txt"), args.binary, source)
        args.candidate.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
        print(f"Benchmark identity stamped for source {source}")
    else:
        if not args.baseline or args.max_slowdown < 0 or args.max_slowdown > 1:
            raise RuntimeError("Comparable baseline and valid slowdown threshold required")
        n = compare(json.loads(args.baseline.read_text()),
                    json.loads(args.candidate.read_text()), args.max_slowdown)
        print(f"PASS: {n} comparable benchmark medians within threshold")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Benchmark qualification FAIL: {error}", file=sys.stderr)
        sys.exit(1)

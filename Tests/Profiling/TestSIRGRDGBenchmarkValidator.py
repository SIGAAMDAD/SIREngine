#!/usr/bin/env python3
"""Negative tests for the GRDG optimized benchmark artifact/regression checker."""

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from ValidateSIRGRDGBenchmarks import REQUIRED


def sample():
    return {
        "context": {
            "host_name": "grdg-matched-host",
            "num_cpus": 8,
            "mhz_per_cpu": 3000,
            "library_build_type": "release",
        },
        "benchmarks": [
            {
                "name": name + "_median",
                "run_type": "aggregate",
                "aggregate_name": "median",
                "real_time": 1000.0,
                "cpu_time": 800.0,
                "time_unit": "ns",
            }
            for name in sorted(REQUIRED)
        ],
    }


def main():
    validator = Path(__file__).resolve().parent / "ValidateSIRGRDGBenchmarks.py"
    with tempfile.TemporaryDirectory() as folder:
        folder = Path(folder)
        candidate = folder / "candidate.json"
        baseline = folder / "baseline.json"

        def check(doc, old, expected_success, label):
            candidate.write_text(json.dumps(doc), encoding="utf-8")
            if old is not None:
                baseline.write_text(json.dumps(old), encoding="utf-8")
            command = [sys.executable, str(validator), str(candidate)]
            if old is not None:
                command.extend(["--baseline", str(baseline), "--max-regression-percent", "20"])
            completed = subprocess.run(command, capture_output=True, text=True, check=False)
            success = completed.returncode == 0
            if success != expected_success:
                raise AssertionError(
                    f"{label}: expected success={expected_success}, got exit={completed.returncode}\n"
                    f"{completed.stdout}\n{completed.stderr}"
                )

        control = sample()
        check(control, None, True, "coverage complete")
        check(control, control, True, "same-baseline no regression")
        damaged = copy.deepcopy(control)
        damaged["benchmarks"].pop()
        check(damaged, None, False, "missing required scenario")
        damaged = copy.deepcopy(control)
        damaged["benchmarks"][0]["real_time"] = -1
        check(damaged, None, False, "negative latency")
        damaged = copy.deepcopy(control)
        damaged["benchmarks"][0]["error_occurred"] = True
        check(damaged, None, False, "benchmark failure")
        damaged = copy.deepcopy(control)
        damaged["benchmarks"][0]["real_time"] = 2000
        check(damaged, control, False, "severe latency regression")
        damaged = copy.deepcopy(control)
        damaged["context"]["host_name"] = "unmatched-host"
        check(damaged, control, False, "incomparable hosts")
        print("PASS: GRDG benchmark validator coverage, failure cases and baseline comparison")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

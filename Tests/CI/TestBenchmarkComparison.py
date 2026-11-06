#!/usr/bin/env python3
"""Comparison and identity negative-path tests for performance evidence."""
import copy
import importlib.util
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[2] / "Build/CI/CompareBenchmarks.py"
spec = importlib.util.spec_from_file_location("sir_benchmark_compare", script)
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class BenchmarkComparisonTests(unittest.TestCase):
    def setUp(self):
        self.base = {
            "context": {
                "cpu_info": [{"name": "fixed-cpu"}],
                "sir_identity": {
                    "host": "fixed-runner",
                    "system": "Linux",
                    "machine": "x86_64",
                    "compiler": "/usr/bin/clang++-23",
                    "compiler_version": "clang version 23.1.5",
                    "build_type": "RelWithDebInfo",
                    "flags": "-O2",
                    "presets_digest": "test",
                    "source_sha": "a" * 40,
                },
            },
            "benchmarks": [
                {"name": "Scheduler/64_median", "aggregate_name": "median",
                 "real_time": 100, "time_unit": "ns"}
            ],
        }
        self.current = copy.deepcopy(self.base)
        self.current["context"]["sir_identity"]["source_sha"] = "b" * 40

    def test_unchanged_workload_passes(self):
        self.assertEqual(compare.compare(self.base, self.current), 1)

    def test_empty_matching_compiler_flags_are_valid(self):
        self.base["context"]["sir_identity"]["flags"] = ""
        self.current["context"]["sir_identity"]["flags"] = ""
        self.assertEqual(compare.compare(self.base, self.current), 1)

    def test_small_slowdown_passes(self):
        self.current["benchmarks"][0]["real_time"] = 110
        self.assertEqual(compare.compare(self.base, self.current), 1)

    def test_runtime_regression_fails(self):
        self.current["benchmarks"][0]["real_time"] = 150
        with self.assertRaisesRegex(RuntimeError, "regression"):
            compare.compare(self.base, self.current)

    def test_different_host_fails(self):
        self.current["context"]["sir_identity"]["host"] = "unknown-host"
        with self.assertRaisesRegex(RuntimeError, "identity: host"):
            compare.compare(self.base, self.current)

    def test_changed_compiler_fails(self):
        self.current["context"]["sir_identity"]["compiler_version"] = "clang 24"
        with self.assertRaisesRegex(RuntimeError, "compiler_version"):
            compare.compare(self.base, self.current)

    def test_missing_workload_fails(self):
        self.current["benchmarks"] = []
        with self.assertRaisesRegex(RuntimeError, "No benchmark medians"):
            compare.compare(self.base, self.current)

    def test_different_cpu_fails(self):
        self.current["context"]["cpu_info"] = [{"name": "other-cpu"}]
        with self.assertRaisesRegex(RuntimeError, "physical CPU"):
            compare.compare(self.base, self.current)


if __name__ == "__main__":
    unittest.main()

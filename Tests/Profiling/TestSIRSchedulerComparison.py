#!/usr/bin/env python3
"""Deterministic regression tests for scheduler benchmark evidence processing."""
import importlib.util
import pathlib
import tempfile
import unittest
import json

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("sir_scheduler_compare", ROOT / "Tools/Profiling/SIRCompareBenchmarks.py")
COMPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPARE)
RUNNER_SPEC = importlib.util.spec_from_file_location("sir_scheduler_runner", ROOT / "Tools/Profiling/SIRRunSchedulerExperiments.py")
RUNNER = importlib.util.module_from_spec(RUNNER_SPEC)
RUNNER_SPEC.loader.exec_module(RUNNER)


def sample(name, duration, unit="ns"):
    return {"name": name + "_median", "run_name": name, "aggregate_name": "median", "real_time": duration,
            "cpu_time": duration, "time_unit": unit}


class SchedulerComparisonTests(unittest.TestCase):
    def test_matched_medians_and_unit_conversion(self):
        old = {"context": {"num_cpus": 8}, "benchmarks": [sample("Example/4", 2, "us")]}
        new = {"context": {"num_cpus": 8}, "benchmarks": [sample("Example/4", 1500, "ns")]}
        result = COMPARE.compare(old, new)
        self.assertEqual(result["matched_count"], 1)
        self.assertEqual(result["results"][0]["delta_pct"], -25)
        self.assertAlmostEqual(result["results"][0]["speedup"], 4 / 3)

    def test_mismatch_rejected(self):
        a = {"context": {"num_cpus": 4}, "benchmarks": [sample("S/1", 1)]}
        b = {"context": {"num_cpus": 8}, "benchmarks": [sample("S/1", 1)]}
        with self.assertRaises(ValueError):
            COMPARE.compare(a, b)
        self.assertIn("num_cpus", COMPARE.compare(a, b, allow_mismatch=True)["context_mismatches"])

    def test_no_medians_rejected(self):
        with self.assertRaises(ValueError):
            COMPARE.medians({"benchmarks": [{"name": "A", "real_time": 12}]})

    def test_invalid_or_duplicate_medians_rejected(self):
        with self.assertRaises(ValueError):
            COMPARE.medians({"benchmarks": [sample("A", 1), sample("A", 2)]})
        with self.assertRaises(ValueError):
            COMPARE.medians({"benchmarks": [sample("A", float("nan"))]})

    def test_profile_rankings_are_separate(self):
        prefix = "SIRBenchmarkExperimentReplay"
        data = {"benchmarks": [sample(f"{prefix}/0/1/32", 10), sample(f"{prefix}/0/4/128", 6),
                               sample(f"{prefix}/1/1/32", 3), sample(f"{prefix}/1/4/128", 4)]}
        result = COMPARE.rank(data, prefix)
        self.assertEqual(result["rankings"]["0"][0]["workers"], 4)
        self.assertEqual(result["rankings"]["1"][0]["workers"], 1)

    def test_runner_rejects_empty_or_skipped_benchmarks(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "result.json"
            path.write_text(json.dumps({"benchmarks": []}), encoding="utf-8")
            self.assertFalse(RUNNER.validate_benchmark_json(path))
            path.write_text(json.dumps({"benchmarks": [{"error_occurred": True, "aggregate_name": "median"}]}),
                            encoding="utf-8")
            self.assertFalse(RUNNER.validate_benchmark_json(path))
            path.write_text(json.dumps({"benchmarks": [sample("A", 100)]}), encoding="utf-8")
            self.assertTrue(RUNNER.validate_benchmark_json(path))

    def test_json_file_io(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "case.json"
            path.write_text(json.dumps({"benchmarks": [sample("A", 14)]}), encoding="utf-8")
            self.assertEqual(COMPARE.load(path)["benchmarks"][0]["run_name"], "A")


if __name__ == "__main__":
    unittest.main()

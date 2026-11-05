#!/usr/bin/env python3
"""Negative coverage-collector acceptance regressions."""

import contextlib
import importlib.util
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("grdg_coverage_collector", HERE / "CollectSIRGRDGCoverage.py")
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


class TestGRDGCoverageCollector(unittest.TestCase):
    def test_negative_branch_counter_is_indexed(self):
        lines = [
            {"line_number": 5, "count": 13, "branches": [
                {"count": 13, "blockno": 7, "fallthrough": True},
                {"count": -1, "blockno": 8, "throw": True},
                {"count": 0, "blockno": 9},
            ]},
            {"line_number": 6, "count": 0, "branches": []},
        ]
        summary = collector.summary(lines)
        details, index = collector.uncovered_details("source.cpp", lines, "a" * 64)
        self.assertEqual(summary["branches"] - summary["covered_branches"], 2)
        self.assertEqual(sum(x["kind"] == "branch" for x in index), 2)
        self.assertEqual(sum(x["kind"] == "line" for x in index), 1)
        self.assertEqual([x["classification"] for x in index if x["kind"] == "branch"],
                         ["negative_gcov_count", "unexecuted_branch"])
        self.assertEqual(details[0]["uncovered_branches"][0]["ordinal"], 1)
        self.assertEqual(details[0]["uncovered_branches"][0]["basic_block"], 8)

    def test_raw_integer_complete_gate_rejects_one_uncovered(self):
        complete = {"executable_lines": 101, "covered_lines": 101,
                    "branches": 101, "covered_branches": 101}
        self.assertEqual(collector.coverage_failures({"x": complete}, 100, 100, True), [])
        almost = {**complete, "covered_branches": 100}
        self.assertTrue(collector.coverage_failures({"x": almost}, 99, 99, True))
        self.assertTrue(collector.coverage_failures({"x": almost}, 100, 100, False))
        almost = {**complete, "covered_lines": 100}
        self.assertTrue(collector.coverage_failures({"x": almost}, 99, 99, True))
        self.assertFalse(collector.coverage_failures({"x": almost}, 99, 99, False))

    def test_missing_gcda_fails_even_with_zero_thresholds(self):
        with tempfile.TemporaryDirectory() as temp:
            build = pathlib.Path(temp) / "build"
            build.mkdir()
            output = pathlib.Path(temp) / "result"
            argv = ["collector", "--build", str(build), "--output", str(output),
                    "--source-commit", "6e2c4bef3ac35b070aeac2f3b524c50bd7804cbd"]
            with mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()), \
                 contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(collector.main(), 1)
            report = json.loads((output / "grdg-coverage.json").read_text())
            self.assertEqual(report["status"], "incomplete")
            self.assertEqual(len(report["missing"]), len(collector.TARGETS))

    def test_missing_source_commit_blocks_complete_gate(self):
        with mock.patch.dict("os.environ", {"GITHUB_SHA": ""}), mock.patch.object(
                collector.subprocess, "run", return_value=mock.Mock(returncode=1, stdout="")):
            self.assertIsNone(collector.source_revision(None))
        self.assertEqual(collector.source_revision("6e2c4bef3ac35b070aeac2f3b524c50bd7804cbd"),
                         "6e2c4bef3ac35b070aeac2f3b524c50bd7804cbd")
        self.assertIsNone(collector.source_revision("not-a-sha"))

    def test_non_numeric_branch_count_fails(self):
        with self.assertRaisesRegex(ValueError, "nonnumeric branch count"):
            collector.uncovered_details("source.cpp", [{
                "line_number": 1, "count": 1, "branches": [{"count": "never"}]}], "b" * 64)


if __name__ == "__main__":
    unittest.main()

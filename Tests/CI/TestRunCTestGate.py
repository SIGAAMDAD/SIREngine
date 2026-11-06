#!/usr/bin/env python3
"""Negative-path tests for source-bound CTest execution evidence."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from unittest import mock

script = Path(__file__).resolve().parents[2] / "Build/CI/RunCTestGate.py"
spec = importlib.util.spec_from_file_location("sir_ctest_gate", script)
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class CTestEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        self.report = Path(self.dir.name) / "results.xml"

    def write_report(self, cases):
        self.report.write_text("<testsuite>" + cases + "</testsuite>", encoding="utf-8")

    def test_complete_result_passes(self):
        self.write_report('<testcase name="unit.first"/><testcase name="unit.second"/>')
        self.assertEqual(gate.verify_results(["unit.first", "unit.second"], self.report, 0), 2)

    def test_empty_selection_fails(self):
        self.write_report("")
        with self.assertRaisesRegex(RuntimeError, "empty"):
            gate.verify_results([], self.report, 0)

    def test_missing_test_fails(self):
        self.write_report('<testcase name="unit.first"/>')
        with self.assertRaisesRegex(RuntimeError, "missing"):
            gate.verify_results(["unit.first", "unit.second"], self.report, 0)

    def test_skipped_required_test_fails(self):
        self.write_report('<testcase name="driver.real"><skipped/></testcase>')
        with self.assertRaisesRegex(RuntimeError, "skipped"):
            gate.verify_results(["driver.real"], self.report, 0)

    def test_failed_test_fails(self):
        self.write_report('<testcase name="unit.first"><failure/></testcase>')
        with self.assertRaisesRegex(RuntimeError, "failed"):
            gate.verify_results(["unit.first"], self.report, 8)

    def test_exit_code_overrides_success_xml(self):
        self.write_report('<testcase name="unit.first"/>')
        with self.assertRaisesRegex(RuntimeError, "exit=8"):
            gate.verify_results(["unit.first"], self.report, 8)

    def test_unexpected_test_fails(self):
        self.write_report('<testcase name="unit.first"/><testcase name="unit.extra"/>')
        with self.assertRaisesRegex(RuntimeError, "extra"):
            gate.verify_results(["unit.first"], self.report, 0)

    def test_repeated_reports_have_distinct_round_paths(self):
        expected = [self.report.with_name(f"results.round-{i:02d}.xml")
                    for i in range(1, 6)]
        actual = [gate.report_for_iteration(self.report, i, 5)
                  for i in range(1, 6)]
        self.assertEqual(actual, expected)
        self.assertEqual(gate.report_for_iteration(self.report, 1, 1), self.report)

    def test_repetition_rejects_skip_in_later_round(self):
        rounds = []

        def fake_run(command, **_kwargs):
            if "--show-only=json-v1" in command:
                return SimpleNamespace(returncode=0, stdout='{"tests":[{"name":"unit.first"}]}',
                                       stderr="")
            report = Path(command[command.index("--output-junit") + 1])
            rounds.append(report)
            if len(rounds) == 2:
                report.write_text('<testsuite><testcase name="unit.first"><skipped/>'
                                  '</testcase></testsuite>')
            else:
                report.write_text('<testsuite><testcase name="unit.first"/>'
                                  '</testsuite>')
            return SimpleNamespace(returncode=0)

        with mock.patch.object(gate.subprocess, "run", side_effect=fake_run):
            with self.assertRaisesRegex(RuntimeError, "round 2/3.*skipped"):
                gate.run_gate(["--test-dir", self.dir.name, "--repeat", "3",
                               "--report", str(self.report)])
        self.assertEqual(len(rounds), 2)

    def test_duplicate_junit_cases_fail(self):
        self.write_report('<testcase name="unit.first"/><testcase name="unit.first"/>')
        with self.assertRaisesRegex(RuntimeError, "duplicate"):
            gate.verify_results(["unit.first"], self.report, 0)


if __name__ == "__main__":
    unittest.main()

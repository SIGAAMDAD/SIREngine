#!/usr/bin/env python3
"""Negative-path tests for source-bound CTest execution evidence."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

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

    def test_duplicate_junit_cases_fail(self):
        self.write_report('<testcase name="unit.first"/><testcase name="unit.first"/>')
        with self.assertRaisesRegex(RuntimeError, "duplicate"):
            gate.verify_results(["unit.first"], self.report, 0)


if __name__ == "__main__":
    unittest.main()

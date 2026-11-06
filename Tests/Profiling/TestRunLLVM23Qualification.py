#!/usr/bin/env python3
"""Negative-path acceptance for the LLVM 23 source qualification gate."""
import importlib.util
import json
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[2] / "Build/Scripts/RunLLVM23Qualification.py"
spec = importlib.util.spec_from_file_location("sir_qualification", script)
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)

class QualificationGateTests(unittest.TestCase):
    def setUp(self):
        self.path = str(Path("/tmp/sir-test-production.cpp").resolve())
        self.fully_covered = {"filename": self.path, "summary": {
            name: {"count": 10, "covered": 10} for name in
            ("lines", "branches", "functions", "regions")}}

    def test_compiler_inventory_preset_is_instrumented(self):
        root = Path(__file__).resolve().parents[2]
        source = json.loads((root / "CMakePresets.json").read_text())
        preset = next(p for p in source["configurePresets"] if p["name"] == "coverage-llvm23")
        options = preset["cacheVariables"]
        self.assertEqual(preset["generator"], "Ninja")
        self.assertEqual(options["CMAKE_EXPORT_COMPILE_COMMANDS"], "ON")
        self.assertIn("-fcoverage-mapping", options["CMAKE_CXX_FLAGS"])
        self.assertIn("-fprofile-instr-generate", options["CMAKE_EXE_LINKER_FLAGS"])
        self.assertEqual(preset["binaryDir"], "${sourceDir}/out/${hostSystemName}/llvm23-qualification")

    def test_full_coverage_passes(self):
        totals = qualification.verify_production_coverage({self.path}, [self.fully_covered])
        self.assertEqual(totals["lines"], (10, 10))

    def test_missing_compiled_source_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Missing compiled"):
            qualification.verify_production_coverage({self.path, "/tmp/other.cpp"}, [self.fully_covered])

    def test_empty_inventory_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Empty compiled"):
            qualification.verify_production_coverage(set(), [self.fully_covered])

    def test_uncovered_branch_fails(self):
        self.fully_covered["summary"]["branches"]["covered"] = 9
        with self.assertRaisesRegex(RuntimeError, "branches=9/10"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_zero_coverage_denominator_fails(self):
        self.fully_covered["summary"]["regions"] = {"count": 0, "covered": 0}
        with self.assertRaisesRegex(RuntimeError, "regions=0/0"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_uncovered_inline_header_fails(self):
        header = str(Path("/tmp/sir-inline.hpp").resolve())
        bad = {"filename": header, "summary": {
            metric: {"count": 1, "covered": 1} for metric in
            ("lines", "branches", "functions", "regions")}}
        bad["summary"]["lines"]["covered"] = 0
        with self.assertRaisesRegex(RuntimeError, "lines=10/11"):
            qualification.verify_production_coverage(
                {self.path}, [self.fully_covered, bad], Path("/tmp"))

    def test_production_header_counted_when_complete(self):
        header = str(Path("/tmp/sir-inline.hpp").resolve())
        extra = {"filename": header, "summary": {
            metric: {"count": 1, "covered": 1} for metric in
            ("lines", "branches", "functions", "regions")}}
        totals = qualification.verify_production_coverage(
            {self.path}, [self.fully_covered, extra], Path("/tmp"))
        self.assertEqual(totals["lines"], (11, 11))

    def test_vendor_header_is_excluded_from_production(self):
        vendor = str(Path("/opt/vendor/external.hpp").resolve())
        extra = {"filename": vendor, "summary": {
            metric: {"count": 1, "covered": 0} for metric in
            ("lines", "branches", "functions", "regions")}}
        totals = qualification.verify_production_coverage(
            {self.path}, [self.fully_covered, extra], Path("/tmp"))
        self.assertEqual(totals["lines"], (10, 10))

    def test_duplicate_production_mapping_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Duplicated production"):
            qualification.verify_production_coverage(
                {self.path}, [self.fully_covered, self.fully_covered], Path("/tmp"))

    def test_missing_counter_fails(self):
        del self.fully_covered["summary"]["functions"]
        with self.assertRaisesRegex(RuntimeError, "Malformed LLVM functions"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

if __name__ == "__main__":
    unittest.main()

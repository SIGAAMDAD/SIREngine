#!/usr/bin/env python3
"""Codecov publishing and README status badges must reflect source-bound measurements."""
import importlib.util
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase, main, mock

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Build/Scripts/RunLLVM23Qualification.py"
spec = importlib.util.spec_from_file_location("qualification_codecov", SCRIPT)
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)


class CodecovPublishingTests(TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name)
        self.scope = self.directory / "Source"
        self.source = self.scope / "Core/Foo.cpp"
        self.source.parent.mkdir(parents=True)
        self.source.write_text("int answer() { return 42; }\n")
        self.mapped = [{"filename": str(self.source)}]
        self.commands = [{"directory": str(self.directory), "file": str(self.source)}]
        self.dest = self.directory / "coverage.lcov"

    def export(self, mapped=None, commands=None):
        return qualification.export_codecov_lcov(
            self.mapped if mapped is None else mapped,
            self.commands if commands is None else commands,
            self.scope, [self.directory / "Foo.o"],
            self.directory / "sample.profdata", self.dest)

    def test_compiled_source_cannot_be_unmapped(self):
        self.dest.write_text("stale")
        with self.assertRaisesRegex(RuntimeError, "complete nonempty production source mapping"):
            self.export(mapped=[])
        self.assertFalse(self.dest.exists())

    def test_empty_compiled_inventory_fails_closed(self):
        with self.assertRaisesRegex(RuntimeError, "complete nonempty production source mapping"):
            self.export(commands=[])

    def test_valid_lcov_is_emitted_from_only_mapped_production_paths(self):
        def exporter(argv, **kwargs):
            self.assertIn("-format=lcov", argv)
            self.assertIn("-sources", argv)
            self.assertIn(str(self.source), argv)
            kwargs["stdout"].write("TN:\nSF:" + str(self.source) + "\nDA:1,1\nend_of_record\n")
            return SimpleNamespace(returncode=0, stderr="")
        with mock.patch.object(qualification.subprocess, "run", side_effect=exporter):
            self.assertEqual(self.export(), self.dest)
        self.assertTrue(self.dest.read_text().startswith("TN:"))

    def test_llvm_warnings_delete_untrusted_lcov(self):
        def exporter(argv, **kwargs):
            kwargs["stdout"].write("TN:\nSF:" + str(self.source) + "\nDA:1,1\nend_of_record\n")
            return SimpleNamespace(returncode=0, stderr="warning: functions have mismatched data")
        with mock.patch.object(qualification.subprocess, "run", side_effect=exporter):
            with self.assertRaisesRegex(RuntimeError, "Untrustworthy Codecov"):
                self.export()
        self.assertFalse(self.dest.exists())

    def test_empty_lcov_is_rejected(self):
        with mock.patch.object(qualification.subprocess, "run",
                               return_value=SimpleNamespace(returncode=0, stderr="")):
            with self.assertRaisesRegex(RuntimeError, "no executed production line inventory"):
                self.export()
        self.assertFalse(self.dest.exists())

    def test_badges_reference_actual_service_branches(self):
        readme = (ROOT / "README.md").read_text()
        self.assertIn("actions/workflows/ci.yml/badge.svg?branch=main", readme)
        self.assertIn("actions/workflows/ci.yml/badge.svg?branch=develop", readme)
        self.assertIn("actions/workflows/nightly.yml/badge.svg?branch=main", readme)
        self.assertIn("codecov.io/gh/SIGAAMDAD/SIREngine/branch/develop/graph/badge.svg", readme)
        self.assertIn("License-MPL%202.0", readme)

    def test_codecov_is_full_scope_and_pinned(self):
        workflow = (ROOT / ".github/workflows/qualification.yml").read_text()
        self.assertIn("codecov/codecov-action@0fb7174895f61a3b6b78fc075e0cd60383518dac", workflow)
        self.assertIn("hashFiles('out/Verification/LLVM23/codecov-production.lcov')", workflow)
        self.assertIn("inputs.system == '' || inputs.system == 'all'", workflow)
        self.assertIn("secrets.CODECOV_TOKEN", workflow)
        self.assertIn("fail_ci_if_error: true", workflow)
        script = SCRIPT.read_text()
        self.assertIn('if name == "all":\n        export_codecov_lcov(', script)
        self.assertIn('REPORT / "codecov-production.lcov"', script)
        self.assertIn('("warning:", "mismatch", "out of date")', script)


if __name__ == "__main__":
    main()

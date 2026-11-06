#!/usr/bin/env python3
"""Regression tests for SIREngine commit and pull-request admission."""
import importlib.util
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Build/CI/CheckChangePolicy.py"
spec = importlib.util.spec_from_file_location("sir_change_policy", SCRIPT)
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)


class ChangePolicyTests(unittest.TestCase):
    def test_normal_conventional_subjects(self):
        self.assertEqual(policy.validate_commit("fix(build): isolate LLVM profile mappings"), [])
        self.assertEqual(policy.validate_commit("ci: restrict redundant PR builds"), [])
        self.assertEqual(policy.validate_commit("refactor(core/jobs): consolidate task API"), [])

    def test_breaking_change_requires_footer(self):
        self.assertTrue(policy.validate_commit("refactor(core)!: change ABI"))
        self.assertEqual(policy.validate_commit(
            "refactor(core)!: change ABI\n\nBREAKING CHANGE: API migration required\n"), [])

    def test_invalid_header_and_formatting(self):
        for subject in ("Merge pull request #42", "FIX(core): bug", "fix(core) bug",
                        "fix(core): bug.", "fix(core): " + "x" * 75):
            with self.subTest(subject=subject):
                self.assertTrue(policy.validate_commit(subject))

    def test_pr_requires_issue_link(self):
        title = "fix(build): protect nightly integration source"
        self.assertEqual(policy.validate_pr(title, "Fixes #314."), [])
        self.assertEqual(policy.validate_pr(title,
                         "Tracks https://github.com/SIGAAMDAD/SIREngine/issues/314"), [])
        self.assertTrue(policy.validate_pr(title, "No tracked issue"))
        self.assertTrue(policy.validate_pr("unstructured title", "Related: #314"))

    def test_cli_rejects_bad_commit_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "COMMIT_EDITMSG"
            path.write_text("bad commit\n", encoding="utf-8")
            result = subprocess.run([sys.executable, str(SCRIPT), "--commit-file", str(path)],
                                    text=True, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Conventional Commit", result.stderr)

    def test_cli_rejects_pr_without_issue(self):
        env = dict(os.environ, SIR_PR_TITLE="fix(core): bounded allocations",
                   SIR_PR_BODY="no issue")
        result = subprocess.run([sys.executable, str(SCRIPT), "--pr-from-env"],
                                env=env, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("authoritative", result.stderr)

    def test_installed_hook_routes_to_same_validator(self):
        hook = (ROOT / "Build/Hooks/commit-msg").read_text()
        self.assertIn("CheckChangePolicy.py", hook)
        self.assertIn("--commit-file", hook)


if __name__ == "__main__":
    unittest.main()

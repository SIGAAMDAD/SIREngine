#!/usr/bin/env python3
"""Reject redundant CI jobs and mismatched nightly source checkouts."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github/workflows"


class WorkflowDisciplineTests(unittest.TestCase):
    def test_ci_cancels_superseded_develop_and_pr_runs(self):
        ci = (WORKFLOWS / "ci.yml").read_text()
        self.assertIn("group: sir-ci-${{ github.event_name }}-${{ github.event.pull_request.number || github.ref }}", ci)
        self.assertIn("github.ref == 'refs/heads/develop'", ci)
        self.assertIn("needs: source", ci)
        self.assertIn("python3 Build/CI/CheckChangePolicy.py --pr-from-env", ci)
        self.assertIn("needs: [source, native, integration-candidate]", ci)

    def test_pr_metadata_edits_rerun_admission_gate(self):
        ci = (WORKFLOWS / "ci.yml").read_text()
        self.assertIn(
            "types: [opened, synchronize, edited, reopened, ready_for_review, converted_to_draft]",
            ci)
        self.assertIn("if: github.event_name == 'pull_request'", ci)
        self.assertIn("SIR_PR_TITLE: ${{ github.event.pull_request.title }}", ci)
        self.assertIn("SIR_PR_BODY: ${{ github.event.pull_request.body }}", ci)
        self.assertIn("run: python3 Build/CI/CheckChangePolicy.py --pr-from-env", ci)

    def test_nightly_freezes_develop_for_all_jobs(self):
        nightly = (WORKFLOWS / "nightly.yml").read_text()
        self.assertIn("  freeze:\n", nightly)
        self.assertIn("ref: develop", nightly)
        self.assertIn('echo "sha=$sha" >> "$GITHUB_OUTPUT"', nightly)
        self.assertEqual(nightly.count("ref: ${{ needs.freeze.outputs.sha }}"), 5)
        self.assertEqual(nightly.count("needs: freeze"), 5)
        self.assertIn("needs: [freeze, source-audit, native, performance, source-dependencies, clang-tooling]", nightly)
        self.assertIn('test "$FROZEN" = success', nightly)

    def test_release_coverage_stricter_than_interim_without_profile_bypass(self):
        qualification = (WORKFLOWS / "qualification.yml").read_text()
        self.assertIn("SIR_COVERAGE_MINIMUM:", qualification)
        self.assertIn("inputs.purpose == 'release'", qualification)
        self.assertIn("github.base_ref == 'main'", qualification)
        self.assertIn("if: success() && (inputs.system == '' || inputs.system == 'all')", qualification)
        self.assertIn("hashFiles('out/Verification/LLVM23/codecov-production.lcov')", qualification)


if __name__ == "__main__":
    unittest.main()

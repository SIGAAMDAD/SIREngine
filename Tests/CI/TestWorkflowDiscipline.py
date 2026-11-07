#!/usr/bin/env python3
"""Verify SIREngine PR admission, nightly revision and qualification ownership."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


class WorkflowDisciplineTests(unittest.TestCase):
    def test_single_ci_and_qualification_entrypoints(self):
        self.assertEqual({p.name for p in WORKFLOWS.glob("*.yml")}, {"ci.yml", "qualification.yml"})
        for path in WORKFLOWS.glob("*.yml"):
            source = path.read_text(encoding="utf-8")
            jobs = [line.strip()[:-1] for line in source.splitlines()
                    if line.startswith("  ") and not line.startswith("   ") and line.endswith(":")]
            jobs = [name for name in jobs if name not in ("schedule", "pull_request", "workflow_dispatch")]
            self.assertEqual(len(jobs), len(set(jobs)), path.name)
            self.assertTrue(jobs, path.name)

    def test_pull_request_changes_recheck_admission(self):
        ci = (WORKFLOWS / "ci.yml").read_text(encoding="utf-8")
        self.assertIn("types: [opened, synchronize, edited, reopened, ready_for_review, converted_to_draft]", ci)
        self.assertIn("if: github.event_name == 'pull_request'", ci)
        self.assertIn("SIR_PR_TITLE: ${{ github.event.pull_request.title }}", ci)
        self.assertIn("SIR_PR_BODY: ${{ github.event.pull_request.body }}", ci)
        self.assertIn("python3 Build/CI/CheckChangePolicy.py --pr-from-env", ci)
        self.assertIn("bash Build/Scripts/FormatAll.sh --ci", ci)
        self.assertIn("python3 -m unittest discover -s Tests/CI", ci)
        self.assertIn("needs: source", ci)
        self.assertIn("needs: [source, native]", ci)

    def test_cancel_stale_pr_and_develop_but_never_main(self):
        ci = (WORKFLOWS / "ci.yml").read_text(encoding="utf-8")
        self.assertIn("github.event.pull_request.number || github.ref", ci)
        self.assertIn("github.ref == 'refs/heads/develop'", ci)
        self.assertNotIn("github.ref != 'refs/heads/main'", ci)

    def test_nightly_uses_one_frozen_develop_revision(self):
        workflow = (WORKFLOWS / "qualification.yml").read_text(encoding="utf-8")
        self.assertEqual(workflow.count("  nightly-revision:\n"), 1)
        self.assertIn("ref: develop", workflow)
        self.assertEqual(workflow.count("ref: ${{ needs.nightly-revision.outputs.sha }}"), 2)
        self.assertIn("needs: [nightly-revision, nightly-native, nightly-benchmark]", workflow)
        self.assertIn("ctest --preset release -L '^nightly:repeat$'", workflow)

    def test_release_coverage_and_codecov_stay_source_bound(self):
        workflow = (WORKFLOWS / "qualification.yml").read_text(encoding="utf-8")
        self.assertIn("SIR_COVERAGE_MINIMUM:", workflow)
        self.assertIn("inputs.purpose == 'release'", workflow)
        self.assertIn("github.base_ref == 'main'", workflow)
        self.assertIn("hashFiles('out/Verification/LLVM23/codecov-production.lcov')", workflow)


if __name__ == "__main__":
    unittest.main()

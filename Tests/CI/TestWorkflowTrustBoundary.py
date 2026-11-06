#!/usr/bin/env python3
"""Prevent default-branch cache poisoning via untrusted revision execution."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"


class WorkflowTrustBoundaryTests(unittest.TestCase):
    def test_no_cross_trust_actions_cache(self):
        for path in list(WORKFLOWS.glob("*.yml")) + list(WORKFLOWS.glob("*.yaml")):
            with self.subTest(path=path.name):
                source = path.read_text()
                self.assertNotRegex(source, r"(?m)^\s*- uses: actions/cache(?:/[^@]+)?@")
                self.assertNotIn("cache-mode: write", source)
                self.assertNotIn("cache-mode: write-only", source)

    def test_manual_dispatch_never_checks_out_caller_sha(self):
        ci = (WORKFLOWS / "ci.yml").read_text()
        qualification = (WORKFLOWS / "qualification.yml").read_text()
        for workflow in (ci, qualification):
            self.assertNotIn("inputs.candidate_sha", workflow)
            self.assertNotIn("inputs.source_sha", workflow)
            self.assertNotRegex(workflow, r"(?m)^\s*ref:\s*\$\{\{\s*inputs\.")
        self.assertIn("ref: ${{ github.sha }}", ci)
        self.assertIn("ref: ${{ github.event.pull_request.head.sha || github.sha }}", qualification)

    def test_hosted_hardware_policy_preserves_local_native_drivers(self):
        qualification = (WORKFLOWS / "qualification.yml").read_text()
        self.assertIn("backend: opengl46", qualification)
        self.assertIn("backend: d3d12", qualification)
        self.assertNotIn("backend: vulkan", qualification)
        self.assertNotIn("audio-backend:", qualification)
        self.assertNotIn("SIR_FMOD_SDK_ROOT", qualification)
        self.assertIn(
            "needs: [authorize, coverage, native, sanitizer, renderer-backends]", qualification)
        self.assertIn(
            "if(SIR_WITH_VULKAN)",
            (ROOT / "Source/Renderer/CMakeLists.txt").read_text())
        self.assertIn(
            "if(SIR_WITH_FMOD)",
            (ROOT / "Source/Audio/CMakeLists.txt").read_text())
        self.assertTrue(
            (ROOT / "Source/Renderer/Drivers/Vulkan/SIRVulkanDevice.cpp").is_file())
        self.assertTrue(
            (ROOT / "Source/Audio/FMOD/SIRFMODRuntime.cpp").is_file())

    def test_negative_patterns_detect_exploitable_examples(self):
        cache_example = "      - uses: actions/cache@v4"
        checkout_example = "          ref: ${{ inputs.source_sha }}"
        self.assertRegex(cache_example, r"(?m)^\s*- uses: actions/cache(?:/[^@]+)?@")
        self.assertRegex(checkout_example, r"(?m)^\s*ref:\s*\$\{\{\s*inputs\.")

    def test_unmerged_qualification_stays_in_pr_security_scope(self):
        qualification = (WORKFLOWS / "qualification.yml").read_text()
        self.assertIn("pull_request:", qualification)
        self.assertIn("types: [labeled, synchronize, reopened, ready_for_review]", qualification)
        self.assertIn("contains(github.event.pull_request.labels.*.name, 'sir-qualify')", qualification)
        self.assertIn("github.event.pull_request.head.repo.full_name == github.repository", qualification)
        self.assertIn("github.event.pull_request.number || inputs.issue", qualification)
        self.assertIn("test \"$AUTHORIZE\" = success", qualification)
        self.assertIn("if: inputs.purpose == 'release'", qualification)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Keep the documented PR acceptance flow synchronized with native CI."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativePRProtocolTests(unittest.TestCase):
    def test_native_source_and_acceptance_template(self):
        template = (ROOT / ".github/PULL_REQUEST_TEMPLATE.md").read_text()
        for required in ("CMake 4.4.5", "FASTBuild 1.20", "clang-format 23.1.5",
                         "sir/required", "CTest/GoogleTest", "LLVM 23.1.5"):
            self.assertIn(required, template)
        self.assertNotIn("SCons", template)

    def test_subsystem_unit_presets(self):
        source = json.loads((ROOT / "CMakePresets.json").read_text())
        presets = {p["name"]: p for p in source["testPresets"]}
        for name in ("core", "resources", "renderer", "terrain", "editor", "scripting",
                     "audio", "mesh", "streaming", "world", "ui"):
            preset = presets[name + "-unit"]
            self.assertEqual(preset["configurePreset"], "dev")
            self.assertEqual(preset["filter"]["include"]["name"], "^" + name + "[.]")
            self.assertEqual(preset["filter"]["include"]["label"], "^kind:unit$")

    def test_stress_not_part_of_pr_cadence(self):
        source = json.loads((ROOT / "CMakePresets.json").read_text())
        presets = {p["name"]: p for p in source["testPresets"]}
        self.assertEqual(presets["nightly-stress"]["filter"]["include"]["label"],
                         "^kind:(stress|soak|fuzz)$")
        self.assertEqual(presets["pr-smoke"]["filter"]["include"]["label"], "^gate:pr$")


if __name__ == "__main__":
    unittest.main()

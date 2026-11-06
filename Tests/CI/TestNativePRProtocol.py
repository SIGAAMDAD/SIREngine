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


    def test_llvm_official_archive_large_decoder_window(self):
        installer = (ROOT / "Build/CI/InstallLLVM23.sh").read_text()
        # LLVM 23.1.5 official tarball declares a 1 GiB Zstandard window.
        self.assertIn("tar --use-compress-program='zstd --long=30' -xf", installer)
        self.assertNotIn("tar --use-compress-program=zstd -xf", installer)


    def test_llvm23_coverage_uses_clang_for_c_and_cpp(self):
        source = json.loads((ROOT / "CMakePresets.json").read_text())
        preset = next(item for item in source["configurePresets"]
                      if item["name"] == "coverage-llvm23")
        flags = preset["cacheVariables"]
        self.assertEqual(flags["CMAKE_C_COMPILER"], "clang-23")
        self.assertEqual(flags["CMAKE_CXX_COMPILER"], "clang++-23")
        self.assertIn("-fprofile-instr-generate", flags["CMAKE_EXE_LINKER_FLAGS"])


if __name__ == "__main__":
    unittest.main()

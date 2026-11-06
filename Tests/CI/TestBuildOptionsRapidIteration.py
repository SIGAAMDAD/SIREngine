#!/usr/bin/env python3
"""Guard the FASTBuild incremental profile and opt-in owned-target warning policy."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RapidIterationBuildTests(unittest.TestCase):
    def test_existing_fastbuild_profiles_preserved(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        configs = {p["name"]: p for p in presets["configurePresets"]}
        builds = {p["name"]: p for p in presets["buildPresets"]}
        self.assertEqual(configs["dev"]["inherits"], "fast-base")
        self.assertEqual(configs["dev"]["cacheVariables"]["SIR_FAST_ITERATION"], "ON")
        self.assertEqual(configs["fast-base"]["generator"], "FASTBuild")
        self.assertEqual(builds["dev"]["nativeToolOptions"], ["-cache"])

    def test_warning_as_error_is_opt_in_and_target_scoped(self):
        source = (ROOT / "Build/CMake/SIRBuildOptions.cmake").read_text()
        self.assertIn('option(SIR_WARNINGS_AS_ERRORS "Treat SIREngine-owned warnings as errors" OFF)', source)
        start = source.index("function(sir_configure_owned_target")
        self.assertEqual(source[:start].count("SIR_WARNINGS_AS_ERRORS"), 1)
        target_scope = source[start:]
        self.assertIn("if(SIR_WARNINGS_AS_ERRORS)", target_scope)
        self.assertIn("target_compile_options(${sirTarget} PRIVATE /WX)", target_scope)
        self.assertIn("target_compile_options(${sirTarget} PRIVATE -Werror)", target_scope)
        self.assertNotIn("add_compile_options(", target_scope)


if __name__ == "__main__":
    unittest.main()

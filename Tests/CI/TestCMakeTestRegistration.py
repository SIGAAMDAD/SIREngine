#!/usr/bin/env python3
"""Verify CTest registration labels, nesting and fail-closed admission."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

HELPER = Path(__file__).resolve().parents[2] / "Build/CMake/SIRTestRegistration.cmake"


class CTestTaxonomyTests(unittest.TestCase):
    def make_project(self, unit, nested=""):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        (root / "Unit/Nested").mkdir(parents=True)
        (root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "project(sir-fixture NONE)\n"
            "enable_testing()\nadd_subdirectory(Unit)\n"
            f'include("{HELPER.as_posix()}")\n'
            'sir_classify_tests("${CMAKE_CURRENT_SOURCE_DIR}/Unit")\n')
        (root / "Unit/CMakeLists.txt").write_text(unit)
        (root / "Unit/Nested/CMakeLists.txt").write_text(nested)
        result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "out"), "-G", "Ninja"],
                                capture_output=True, text=True, check=False)
        return root, result

    def test_nested_labels_and_stress_cadence(self):
        root, result = self.make_project(
            'add_test(NAME core.smoke COMMAND ${CMAKE_COMMAND} -E echo ok)\n'
            'add_test(NAME core.heavy COMMAND ${CMAKE_COMMAND} -E echo ok)\n'
            'set_tests_properties(core.heavy PROPERTIES LABELS "kind:stress;gate:pr")\n'
            "add_subdirectory(Nested)\n",
            'add_test(NAME renderer.opengl46.smoke COMMAND ${CMAKE_COMMAND} -E echo ok)\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        report = subprocess.run(["ctest", "--test-dir", str(root / "out"), "--show-only=json-v1"],
                                capture_output=True, text=True, check=True)
        cases = {x["name"]: x for x in json.loads(report.stdout)["tests"]}
        self.assertEqual(set(cases), {"core.smoke", "core.heavy", "renderer.opengl46.smoke"})
        props = {name: {p["name"]: p["value"] for p in case["properties"]}
                 for name, case in cases.items()}
        labels = set(props["core.smoke"]["LABELS"])
        self.assertTrue({"gate:pr", "kind:unit", "resource:cpu", "subsystem:core"} <= labels)
        self.assertTrue(any(x.startswith("platform:") for x in labels))
        self.assertIn("kind:stress", props["core.heavy"]["LABELS"])
        self.assertNotIn("gate:pr", props["core.heavy"]["LABELS"])
        self.assertIn("resource:gpu", props["renderer.opengl46.smoke"]["LABELS"])
        self.assertEqual(props["renderer.opengl46.smoke"]["RESOURCE_LOCK"], ["sir-gpu"])

    def test_source_owned_ctest_targets_are_reachable(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "CMakeLists.txt").read_text()
        enable = source.index("enable_testing()")
        configure_source = source.index("add_subdirectory(Source)")
        self.assertLess(enable, configure_source)
        self.assertIn('sir_classify_tests("${PROJECT_SOURCE_DIR}/Source")', source)
        d3d10 = (root / "Source/Renderer/Drivers/D3D10/CMakeLists.txt").read_text()
        self.assertIn("add_test(NAME renderer.d3d10.abi-scaffold COMMAND SIRD3D10ABITests)", d3d10)

    def test_optimizer_worker_usage_profile_registration(self):
        root = Path(__file__).resolve().parents[2]
        scripting = (root / "Tests/Unit/ScriptingTests.cmake").read_text()
        usage = (root / "Tests/Unit/SIRScriptOptimizerWorkerUsage.cmake").read_text()
        self.assertIn("add_test(NAME scripting.optimizer-worker-usage", scripting)
        self.assertIn("$<TARGET_FILE:SIRScriptOptimizerWorker>", scripting)
        self.assertIn('COMMAND "${SIR_WORKER}"', usage)
        self.assertIn('STREQUAL "2"', usage)
        self.assertIn("SIRScriptOptimizerWorker: usage", usage)

    def test_source_test_registration_in_nested_directory(self):
        root, result = self.make_project(
            "add_subdirectory(Nested)\n",
            'add_test(NAME renderer.d3d10.abi-scaffold COMMAND ${CMAKE_COMMAND} -E echo ok)\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        report = subprocess.run(
            ["ctest", "--test-dir", str(root / "out"), "--show-only=json-v1"],
            capture_output=True, text=True, check=True)
        self.assertIn("renderer.d3d10.abi-scaffold",
                      {case["name"] for case in json.loads(report.stdout)["tests"]})

    def test_duplicate_names_fail(self):
        _, result = self.make_project(
            "add_test(NAME core.duplicate COMMAND ${CMAKE_COMMAND} -E echo ok)\n"
            "add_subdirectory(Nested)\n",
            "add_test(NAME core.duplicate COMMAND ${CMAKE_COMMAND} -E echo ok)\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Duplicate registered CTest name", result.stderr)

    def test_conflicting_kinds_fail(self):
        _, result = self.make_project(
            "add_test(NAME core.conflict COMMAND ${CMAKE_COMMAND} -E echo ok)\n"
            'set_tests_properties(core.conflict PROPERTIES LABELS "kind:unit;kind:stress")\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Conflicting CTest kind labels", result.stderr)


if __name__ == "__main__":
    unittest.main()

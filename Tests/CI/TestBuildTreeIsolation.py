#!/usr/bin/env python3
"""Guard out-of-source CMake configuration and generated project files."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GUARD = ROOT / "Build/CMake/SIRBuildTreeGuard.cmake"


class BuildTreeIsolationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / "repo"
        self.repo.mkdir()
        (self.repo / "Build/CMake").mkdir(parents=True)
        shutil.copyfile(GUARD, self.repo / "Build/CMake/SIRBuildTreeGuard.cmake")
        (self.repo / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "include(Build/CMake/SIRBuildTreeGuard.cmake)\n"
            "project(SIRTest NONE)\n")
        world = self.repo / "Source/World"
        world.mkdir(parents=True)
        (world / "CMakeLists.txt").write_text(
            'include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")\n'
            'project(DisallowedStandalone NONE)\n')
        standalone = self.repo / "Tests/Standalone/MeshCache"
        standalone.mkdir(parents=True)
        (standalone / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            'include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")\n'
            "project(SIRMeshCacheStandalone NONE)\n")

    def configure(self, source, binary):
        return subprocess.run(
            ["cmake", "-S", str(source), "-B", str(binary), "-G", "Ninja"],
            capture_output=True, text=True, check=False)

    def test_repo_root_in_source_rejected(self):
        result = self.configure(self.repo, self.repo)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not be generated in source", result.stderr)

    def test_nested_source_build_rejected(self):
        result = self.configure(self.repo, self.repo / "Source/World")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not be generated in source", result.stderr)

    def test_direct_world_configuration_rejected(self):
        result = self.configure(self.repo / "Source/World", Path(self.temp.name) / "world-out")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("subdirectories are not independently", result.stderr)

    def test_out_preserves_mirrored_tree(self):
        binary = self.repo / "out/Windows/vs2022"
        result = self.configure(self.repo, binary)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((binary / "CMakeCache.txt").exists())

    def test_external_build_allowed(self):
        result = self.configure(self.repo, Path(self.temp.name) / "external")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_standalone_mesh_requires_external_build_tree(self):
        standalone = self.repo / "Tests/Standalone/MeshCache"
        success = self.configure(standalone, self.repo / "out/Linux/mesh-cache")
        self.assertEqual(success.returncode, 0, success.stderr)
        failed = self.configure(standalone, standalone)
        self.assertNotEqual(failed.returncode, 0)

    def test_symlinked_build_into_source_rejected(self):
        alias = Path(self.temp.name) / "misdirected-out"
        try:
            alias.symlink_to(self.repo / "Source/World", target_is_directory=True)
        except (NotImplementedError, OSError):
            self.skipTest("symlink unavailable")
        result = self.configure(self.repo, alias)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must not be generated in source", result.stderr)

    def test_ci_checks_develop_and_windows_nightly_msvc(self):
        ci = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn("branches: [main, develop]", ci)
        nightly = (ROOT / ".github/workflows/nightly.yml").read_text()
        self.assertIn("ilammy/msvc-dev-cmd@0b201ec74fa43914dc39ae48a89fd1d8cb592756", nightly)
        self.assertIn("if: runner.os == 'Windows'", nightly)

    def test_source_module_guard_in_all_engine_modules(self):
        for cmake_file in (ROOT / "Source").rglob("CMakeLists.txt"):
            self.assertIn("SIRBuildTreeGuard.cmake", cmake_file.read_text(), str(cmake_file))
        for cmake_file in (ROOT / "Tools").rglob("CMakeLists.txt"):
            self.assertIn("SIRBuildTreeGuard.cmake", cmake_file.read_text(), str(cmake_file))
        for cmake_file in (ROOT / "Tests").rglob("CMakeLists.txt"):
            self.assertIn("SIRBuildTreeGuard.cmake", cmake_file.read_text(), str(cmake_file))

    def test_presets_are_out_of_source(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        by_name = {item["name"]: item for item in presets["configurePresets"]}
        for preset in presets["configurePresets"]:
            resolved = preset
            while "binaryDir" not in resolved:
                inherited = resolved.get("inherits")
                self.assertIsInstance(inherited, str)
                resolved = by_name[inherited]
            self.assertEqual(resolved["binaryDir"],
                             "${sourceDir}/out/${hostSystemName}/${presetName}")
        visual_studio = next(p for p in presets["configurePresets"] if p["name"] == "vs2022")
        self.assertEqual(visual_studio["generator"], "Visual Studio 17 2022")
        self.assertEqual(visual_studio["architecture"], "x64")


if __name__ == "__main__":
    unittest.main()

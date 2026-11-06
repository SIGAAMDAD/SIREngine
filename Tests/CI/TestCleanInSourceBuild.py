#!/usr/bin/env python3
"""Regression checks for bounded source-tree CMake cleanup."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "Build/Scripts/CleanInSourceBuild.py"
spec = importlib.util.spec_from_file_location("sir_clean", MODULE)
clean = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clean)


class CleanInSourceBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        subprocess.run(["git", "-C", str(self.root), "init", "-q"], check=True)
        (self.root / "CMakeLists.txt").write_text("project(SIR)\n")
        source = self.root / "Source/World"
        source.mkdir(parents=True)
        (self.root / "Source/CMakeLists.txt").write_text("add_subdirectory(World)\n")
        (source / "CMakeLists.txt").write_text("add_library(World)\n")
        (source / "Main.cpp").write_text("// authored source\n")
        (source / "CMakeFiles").mkdir()
        (source / "CMakeFiles" / "generated.cmake").write_text("generated\n")
        (source / "SIRWorld.dir").mkdir()
        (source / "SIRWorld.vcxproj").write_text("generated\n")
        (source / "cmake_install.cmake").write_text("generated\n")
        self.source = source

    def test_dry_run_preserves_all_files(self):
        planned = clean.plan_clean(self.root)
        self.assertIn(self.source / "SIRWorld.vcxproj", planned)
        self.assertIn(self.source / "SIRWorld.dir", planned)
        self.assertIn(self.source / "CMakeFiles", planned)
        self.assertEqual(clean.main(["--root", str(self.root)]), 0)
        self.assertTrue((self.source / "SIRWorld.vcxproj").exists())
        self.assertTrue((self.source / "Main.cpp").exists())

    def test_apply_removes_only_generated_files(self):
        self.assertEqual(clean.main(["--root", str(self.root), "--apply"]), 0)
        self.assertFalse((self.source / "SIRWorld.dir").exists())
        self.assertFalse((self.source / "CMakeFiles").exists())
        self.assertFalse((self.source / "SIRWorld.vcxproj").exists())
        self.assertTrue((self.source / "CMakeLists.txt").exists())
        self.assertTrue((self.source / "Main.cpp").exists())

    def test_tracked_generated_name_is_preserved(self):
        subprocess.run(["git", "-C", str(self.root), "add", "Source/World/SIRWorld.vcxproj"],
                       check=True)
        planned = clean.plan_clean(self.root)
        self.assertNotIn(self.source / "SIRWorld.vcxproj", planned)
        self.assertIn(self.source / "SIRWorld.dir", planned)


if __name__ == "__main__":
    unittest.main()

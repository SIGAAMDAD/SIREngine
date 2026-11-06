#!/usr/bin/env python3
"""Negative-path acceptance for the LLVM 23 source qualification gate."""
import importlib.util
import json
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[2] / "Build/Scripts/RunLLVM23Qualification.py"
spec = importlib.util.spec_from_file_location("sir_qualification", script)
qualification = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qualification)

class QualificationGateTests(unittest.TestCase):
    def setUp(self):
        self.path = str(Path("/tmp/sir-test-production.cpp").resolve())
        self.fully_covered = {"filename": self.path, "summary": {
            name: {"count": 10, "covered": 10} for name in
            ("lines", "branches", "functions", "regions")}}

    def test_compiler_inventory_preset_is_instrumented(self):
        root = Path(__file__).resolve().parents[2]
        source = json.loads((root / "CMakePresets.json").read_text())
        preset = next(p for p in source["configurePresets"] if p["name"] == "coverage-llvm23")
        options = preset["cacheVariables"]
        self.assertEqual(preset["generator"], "Ninja")
        self.assertEqual(options["CMAKE_EXPORT_COMPILE_COMMANDS"], "ON")
        self.assertIn("-fcoverage-mapping", options["CMAKE_CXX_FLAGS"])
        self.assertIn("-fprofile-instr-generate", options["CMAKE_EXE_LINKER_FLAGS"])
        self.assertEqual(preset["binaryDir"], "${sourceDir}/out/${hostSystemName}/llvm23-qualification")

    def test_linked_binary_inventory_rejects_relocatable_and_cmake_probe_files(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            build = Path(temp)
            def elf(path, elf_kind=3, executable=True):
                path.parent.mkdir(parents=True, exist_ok=True)
                data = b"\x7fELF" + bytes((2, 1)) + bytes(10) + elf_kind.to_bytes(2, "little")
                path.write_bytes(data)
                path.chmod(0o755 if executable else 0o644)
                return path

            accepted = elf(build / "Tests/Unit/SIRCoreTests")
            shared = elf(build / "Source/libSIRPlugin.so")
            elf(build / "Source/SIRSource.cpp.o", elf_kind=1)
            elf(build / "CMakeFiles/3.23/CompilerIdCXX/a.out")
            elf(build / "_deps/vendor/fixture")
            elf(build / "Tests/Unit/denied-executable", executable=False)
            self.assertEqual(qualification.linked_coverage_binaries(build),
                             sorted([accepted, shared]))

    def test_linked_binary_inventory_refuses_empty_denominator(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            with self.assertRaisesRegex(RuntimeError, "No linked instrumented"):
                qualification.linked_coverage_binaries(temp)

    def test_real_llvm23_inline_dummy_object_vs_linked_binary(self):
        import os
        import shutil
        import subprocess
        from tempfile import TemporaryDirectory
        names = ("clang++-23", "llvm-cov-23", "llvm-profdata-23")
        if any(not shutil.which(name) for name in names):
            self.skipTest("Pinned LLVM 23 compiler and coverage tools unavailable")
        with TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "inline.h").write_text(
                "inline int sirInline(int x) { return x + 1; }\n")
            (root / "unused.cpp").write_text(
                '#include "inline.h"\nint sirUnused() { return 1; }\n')
            (root / "used.cpp").write_text(
                '#include "inline.h"\nint main() { return sirInline(3) == 4 ? 0 : 1; }\n')
            for stem in ("unused", "used"):
                subprocess.run(["clang++-23", "-O0", "-fprofile-instr-generate",
                                "-fcoverage-mapping", "-c", f"{stem}.cpp",
                                "-o", f"{stem}.o"], cwd=root, check=True,
                               capture_output=True, text=True)
            subprocess.run(["clang++-23", "-fprofile-instr-generate",
                            "-fcoverage-mapping", "unused.o", "used.o",
                            "-o", "fixture"], cwd=root, check=True,
                           capture_output=True, text=True)
            env = dict(os.environ, LLVM_PROFILE_FILE=str(root / "run.profraw"))
            subprocess.run([str(root / "fixture")], cwd=root, env=env,
                           check=True, capture_output=True, text=True)
            subprocess.run(["llvm-profdata-23", "merge", "run.profraw",
                            "-o", "run.profdata"], cwd=root, check=True,
                           capture_output=True, text=True)
            objects = subprocess.run(["llvm-cov-23", "export", "unused.o",
                                      "-object=used.o", "-instr-profile=run.profdata",
                                      "-format=text", "--dump"], cwd=root,
                                     check=True, capture_output=True, text=True)
            self.assertIn("functions have mismatched data", objects.stderr)
            self.assertIn("with hash = 0x0", objects.stderr)
            linked = subprocess.run(["llvm-cov-23", "export", "fixture",
                                     "-instr-profile=run.profdata", "-format=text"],
                                    cwd=root, check=True, capture_output=True, text=True)
            self.assertNotIn("mismatched data", linked.stderr)
            data = json.loads(linked.stdout)
            mapped = {Path(item["filename"]).name for item in data["data"][0]["files"]}
            self.assertIn("unused.cpp", mapped)
            self.assertIn("used.cpp", mapped)

    def test_unprofiled_binary_reports_production_functions(self):
        root = Path("/tmp/sir-profile-scope").resolve()
        data = {"functions": [
            {"name": "out_of_scope", "count": 0, "filenames": ["/vendor/test.cpp"]},
            {"name": "covered", "count": 3, "filenames": [str(root / "Source/used.cpp")]},
            {"name": "unprofiled", "count": 0, "filenames": [str(root / "Source/missing.cpp")]}
        ]}
        result = qualification.find_zero_count_functions(data, root / "Source")
        self.assertEqual([item["name"] for item in result], ["unprofiled"])

    def test_no_mismatch_candidates_do_not_pass_coverage(self):
        root = Path("/tmp/sir-profile-scope").resolve()
        self.assertEqual(qualification.find_zero_count_functions({"functions": []}, root), [])
        self.fully_covered["summary"]["lines"]["covered"] = 0
        with self.assertRaisesRegex(RuntimeError, "lines=0/10"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_full_coverage_passes(self):
        totals = qualification.verify_production_coverage({self.path}, [self.fully_covered])
        self.assertEqual(totals["lines"], (10, 10))

    def test_ninety_percent_passes_and_eighty_nine_percent_fails(self):
        names = ("lines", "branches", "functions", "regions")
        for name in names:
            self.fully_covered["summary"][name]["covered"] = 9
        totals = qualification.verify_production_coverage({self.path}, [self.fully_covered])
        self.assertEqual(totals["lines"], (9, 10))
        for name in names:
            with self.subTest(metric=name):
                self.fully_covered["summary"][name] = {"count": 100, "covered": 89}
                with self.assertRaisesRegex(RuntimeError, f"{name}=89/100"):
                    qualification.verify_production_coverage({self.path}, [self.fully_covered])
                self.fully_covered["summary"][name] = {"count": 10, "covered": 9}

    def test_fraction_below_ninety_fails_without_rounding(self):
        self.fully_covered["summary"]["regions"] = {"count": 101, "covered": 90}
        with self.assertRaisesRegex(RuntimeError, "regions=90/101"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_missing_compiled_source_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Missing compiled"):
            qualification.verify_production_coverage({self.path, "/tmp/other.cpp"}, [self.fully_covered])

    def test_empty_inventory_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Empty compiled"):
            qualification.verify_production_coverage(set(), [self.fully_covered])

    def test_uncovered_branch_fails(self):
        self.fully_covered["summary"]["branches"]["covered"] = 8
        with self.assertRaisesRegex(RuntimeError, "branches=8/10"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_zero_coverage_denominator_fails(self):
        self.fully_covered["summary"]["regions"] = {"count": 0, "covered": 0}
        with self.assertRaisesRegex(RuntimeError, "regions=0/0"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])

    def test_uncovered_inline_header_fails(self):
        header = str(Path("/tmp/sir-inline.hpp").resolve())
        bad = {"filename": header, "summary": {
            metric: {"count": 1, "covered": 1} for metric in
            ("lines", "branches", "functions", "regions")}}
        bad["summary"]["lines"]["count"] = 2
        bad["summary"]["lines"]["covered"] = 0
        with self.assertRaisesRegex(RuntimeError, "lines=10/12"):
            qualification.verify_production_coverage(
                {self.path}, [self.fully_covered, bad], Path("/tmp"))

    def test_production_header_counted_when_complete(self):
        header = str(Path("/tmp/sir-inline.hpp").resolve())
        extra = {"filename": header, "summary": {
            metric: {"count": 1, "covered": 1} for metric in
            ("lines", "branches", "functions", "regions")}}
        totals = qualification.verify_production_coverage(
            {self.path}, [self.fully_covered, extra], Path("/tmp"))
        self.assertEqual(totals["lines"], (11, 11))

    def test_vendor_header_is_excluded_from_production(self):
        vendor = str(Path("/opt/vendor/external.hpp").resolve())
        extra = {"filename": vendor, "summary": {
            metric: {"count": 1, "covered": 0} for metric in
            ("lines", "branches", "functions", "regions")}}
        totals = qualification.verify_production_coverage(
            {self.path}, [self.fully_covered, extra], Path("/tmp"))
        self.assertEqual(totals["lines"], (10, 10))

    def test_duplicate_production_mapping_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Duplicated production"):
            qualification.verify_production_coverage(
                {self.path}, [self.fully_covered, self.fully_covered], Path("/tmp"))

    def test_missing_counter_fails(self):
        del self.fully_covered["summary"]["functions"]
        with self.assertRaisesRegex(RuntimeError, "Malformed LLVM functions"):
            qualification.verify_production_coverage({self.path}, [self.fully_covered])


    def test_compiler_inventory_resolves_relative_build_directories(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "Source/Core/SIRExample.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// source")
            commands = [{"directory": "out/build", "file": "../../Source/Core/SIRExample.cpp"}]
            self.assertEqual(qualification.compiled_production_sources(commands, root), {str(source)})

    def test_core_memory_resources_are_qualified_independently(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            paths = [root / "Source/Core/SIRCore.cpp",
                     root / "Source/Core/Memory/SIRMemory.cpp",
                     root / "Source/Resources/SIRResources.cpp"]
            commands = [{"directory": str(root), "file": str(path)} for path in paths]
            files = [{"filename": str(path), "summary": {
                metric: {"covered": 9, "count": 10}
                for metric in ("lines", "branches", "functions", "regions")}}
                for path in paths]
            totals = qualification.verify_group_coverage(commands, files, root)
            self.assertEqual(set(totals), {"Core", "Memory", "Resources"})
            self.assertEqual(totals["Memory"]["branches"], (9, 10))
            files[1]["summary"]["regions"]["covered"] = 8
            with self.assertRaisesRegex(RuntimeError, "Memory:.*regions=8/10"):
                qualification.verify_group_coverage(commands, files, root)
            files[1]["summary"]["regions"]["covered"] = 9
            files[0]["summary"]["lines"]["covered"] = 8
            with self.assertRaisesRegex(RuntimeError, "Core:.*lines=8/10"):
                qualification.verify_group_coverage(commands, files, root)

    def test_group_qualification_rejects_missing_compiled_scope(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            core = root / "Source/Core/SIRCore.cpp"
            commands = [{"directory": str(root), "file": str(core)}]
            files = [{"filename": str(core), "summary": {
                metric: {"covered": 10, "count": 10}
                for metric in ("lines", "branches", "functions", "regions")}}]
            with self.assertRaisesRegex(RuntimeError, "Memory: empty compiled"):
                qualification.verify_group_coverage(commands, files, root)

    def test_group_qualification_rejects_missing_profile_mapping(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            paths = [root / "Source/Core/SIRCore.cpp",
                     root / "Source/Core/Memory/SIRMemory.cpp",
                     root / "Source/Resources/SIRResources.cpp"]
            commands = [{"directory": str(root), "file": str(path)} for path in paths]
            files = [{"filename": str(path), "summary": {
                metric: {"covered": 10, "count": 10}
                for metric in ("lines", "branches", "functions", "regions")}}
                for path in (paths[0], paths[2])]
            with self.assertRaisesRegex(RuntimeError, "Memory: Missing compiled"):
                qualification.verify_group_coverage(commands, files, root)


    def test_objects_come_from_matching_compiler_outputs(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "out/llvm"
            build.mkdir(parents=True)
            obj = build / "core.o"
            obj.write_bytes(b"test object")
            commands = [{"directory": str(build), "file": "../../Source/Core/SIRCore.cpp",
                         "command": "clang++ -c ../../Source/Core/SIRCore.cpp -o core.o"}]
            objects = qualification.compiled_production_objects(
                commands, root, build, root / "Source/Core")
            self.assertEqual(objects, [obj])
            with self.assertRaisesRegex(RuntimeError, "No compiled production object"):
                qualification.compiled_production_objects(
                    commands, root, build, root / "Source/Resources")

    def test_missing_compiler_object_and_unsafe_paths_fail_closed(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "out/llvm"
            build.mkdir(parents=True)
            entry = {"directory": str(build), "file": "../../Source/Core/SIRCore.cpp",
                     "arguments": ["clang++", "-c", "SIRCore.cpp", "-o", "missing.o"]}
            with self.assertRaisesRegex(RuntimeError, "Missing or non-build"):
                qualification.compiled_production_objects(
                    [entry], root, build, root / "Source/Core")
            outside = root / "external.o"
            outside.write_bytes(b"not in compiler build tree")
            entry["output"] = str(outside)
            with self.assertRaisesRegex(RuntimeError, "Missing or non-build"):
                qualification.compiled_production_objects(
                    [entry], root, build, root / "Source/Core")


    def test_combined_scope_requires_both_distinct_production_object_sets(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "out/llvm"
            build.mkdir(parents=True)
            names = ["Source/Core/SIRCore.cpp", "Source/Resources/SIRResourceGraph.cpp",
                     "Source/Audio/SIRAudio.cpp"]
            commands = []
            for i, name in enumerate(names):
                obj = build / f"candidate{i}.o"
                obj.write_bytes(b"source object")
                commands.append({"directory": str(build), "file": f"../../{name}",
                                 "output": obj.name})
            core = qualification.compiled_production_objects(
                commands, root, build, root / "Source/Core")
            resources = qualification.compiled_production_objects(
                commands, root, build, root / "Source/Resources")
            self.assertEqual([obj.name for obj in core + resources],
                             ["candidate0.o", "candidate1.o"])


    def test_diagnostic_reports_missed_paths_without_granting_pass(self):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as temp:
            root = Path(temp)
            paths = [root / "Source/Core/Owned.cpp", root / "Source/Core/Memory/Owned.cpp",
                     root / "Source/Resources/Owned.cpp"]
            commands = [{"directory": str(root), "file": str(path)} for path in paths]
            mapped = [{"filename": str(path), "summary": {
                metric: {"covered": 6, "count": 10}
                for metric in ("lines", "branches", "functions", "regions")}}
                for path in paths]
            # Keep Core and Resources above threshold to isolate Memory's failure.
            for item in (mapped[0], mapped[2]):
                for metric in ("lines", "branches", "functions", "regions"):
                    item["summary"][metric]["covered"] = 10
            diagnostic = qualification.diagnostic_group_totals(mapped, commands, root)
            self.assertEqual(set(diagnostic), {"Core", "Memory", "Resources"})
            self.assertEqual(diagnostic["Memory"]["status"], "DIAGNOSTIC_ONLY")
            self.assertEqual(diagnostic["Memory"]["metrics"]["branches"]["covered"], 6)
            with self.assertRaisesRegex(RuntimeError, "Memory:.*branches=6/10"):
                qualification.verify_group_coverage(commands, mapped, root)
            diagnostic = qualification.diagnostic_group_totals(mapped[:-1], commands, root)
            self.assertEqual(diagnostic["Resources"]["status"], "INCOMPLETE")


    def test_profile_mismatch_summary_keeps_all_hash_conflicts_visible(self):
        records = [
            {"name": "SIRInline", "hash": "0"},
            {"name": "SIRInline", "hash": "0"},
            {"name": "main", "hash": "ef545af72c34ffd"},
            {"name": "driverABI", "hash": "f137fe0d72bd235"},
        ]
        summary = qualification.summarize_profile_mismatches(records)
        self.assertEqual(summary["total_records"], 4)
        self.assertEqual(summary["zero_hash_records"], 2)
        self.assertEqual(summary["zero_hash_symbols"], [{"name": "SIRInline", "count": 2}])
        self.assertEqual(summary["nonzero_hash_records"], 2)
        self.assertEqual(
            {item["name"] for item in summary["nonzero_hash_conflicts"]},
            {"main", "driverABI"})
        self.assertEqual(sum(item["count"] for item in
                             summary["nonzero_hash_conflicts"]), 2)

    def test_invalid_profile_mismatch_record_fails_closed(self):
        with self.assertRaises((RuntimeError, ValueError, KeyError)):
            qualification.summarize_profile_mismatches(
                [{"name": "SIRInline", "hash": "invalid hash"}])

    def test_profile_hash_parser_distinguishes_misses_from_unexecuted_paths(self):
        warning = ("warning: 2 functions have mismatched data\n"
                   "hash-mismatch: No profile record found for '_ZN7Example3runEv' with hash = 0xAb5\n"
                   "hash-mismatch: No profile record found for 'main' with hash = 0x0\n"
                   "warning: arbitrary additional warning\n")
        self.assertEqual(qualification.parse_profile_hash_mismatches(warning), [
            {"name": "_ZN7Example3runEv", "hash": "ab5"},
            {"name": "main", "hash": "0"}])
        self.assertEqual(qualification.parse_profile_hash_mismatches(
            "warning: 2 functions have mismatched data\n"), [])


    def test_profile_isolation_accepts_only_compiler_dummy_zero_hashes(self):
        from types import SimpleNamespace
        def run(command, **kwargs):
            return SimpleNamespace(returncode=0, stderr=(
                "hash-mismatch: No profile record found for 'unusedInline' with hash = 0x0\n"
                "warning: 1 functions have mismatched data\n"))
        self.assertTrue(qualification.validate_binary_profile_isolation(
            [Path("/tmp/a"), Path("/tmp/b")], Path("/tmp/all.profdata"), runner=run))

    def test_profile_isolation_rejects_nonzero_stale_main_and_graphics_hashes(self):
        from types import SimpleNamespace
        for symbol in ("main", "SIRGetGraphicsDriverABI"):
            with self.subTest(symbol=symbol):
                def run(command, **kwargs):
                    return SimpleNamespace(returncode=0, stderr=(
                        f"hash-mismatch: No profile record found for '{symbol}' with hash = 0xAc12\n"
                        "warning: 1 functions have mismatched data\n"))
                with self.assertRaisesRegex(RuntimeError, "Unresolved binary/profile"):
                    qualification.validate_binary_profile_isolation(
                        [Path("/tmp/stale")], Path("/tmp/all.profdata"), runner=run)

    def test_profile_isolation_rejects_unrecognized_warnings_and_empty_input(self):
        from types import SimpleNamespace
        def run(command, **kwargs):
            return SimpleNamespace(returncode=0, stderr="warning: profile out of date\n")
        with self.assertRaisesRegex(RuntimeError, "Unresolved binary/profile"):
            qualification.validate_binary_profile_isolation(
                [Path("/tmp/a")], Path("/tmp/all.profdata"), runner=run)
        with self.assertRaisesRegex(RuntimeError, "No linked binaries"):
            qualification.validate_binary_profile_isolation([], Path("/tmp/all.profdata"))
        self.assertFalse(qualification.validated_aggregate_warning("warning: profile out of date"))
        self.assertTrue(qualification.validated_aggregate_warning(
            "warning: 300 functions have mismatched data\n"))

    def test_codecov_publication_occurs_after_production_coverage_acceptance(self):
        source = script.read_text().split("def qualify():", 1)[1]
        self.assertLess(source.find("totals = verify_production_coverage("),
                        source.find("export_codecov_lcov(data["))

if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Safely inspect and remove misplaced CMake-generated source-tree artifacts.

Defaults to dry-run. Only untracked generated CMake/Visual Studio items are eligible;
tracked source and independent project files are always preserved.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

FILES = {
    "CMakeCache.txt", "cmake_install.cmake", "CTestTestfile.cmake",
    "CPackConfig.cmake", "CPackSourceConfig.cmake", "build.ninja",
    ".ninja_deps", ".ninja_log", "compile_commands.json", "Makefile",
}
DIRS = {"CMakeFiles", "Testing", "_CPack_Packages"}
SUFFIXES = (".vcxproj", ".vcxproj.filters", ".sln")


def plan_clean(root):
    result = subprocess.run(["git", "-C", str(root), "ls-files", "-z"],
                            capture_output=True, check=True)
    tracked = {value.decode("utf-8") for value in result.stdout.split(b"\0") if value}
    targets = []
    for source in (root / "Source", root / "Tests", root / "Tools"):
        if not source.is_dir():
            continue
        for cmake_file in source.rglob("CMakeLists.txt"):
            directory = cmake_file.parent
            if directory.is_symlink():
                continue
            if not any((directory / value).exists() for value in (
                "CMakeFiles", "CMakeCache.txt", "cmake_install.cmake")):
                continue
            for candidate in directory.iterdir():
                if candidate.is_symlink():
                    continue
                if not (candidate.name in FILES or candidate.name in DIRS or
                        candidate.name.endswith(SUFFIXES) or
                        (candidate.is_dir() and candidate.name.endswith(".dir"))):
                    continue
                relative = candidate.relative_to(root).as_posix()
                if relative in tracked or any(p.startswith(relative + "/") for p in tracked):
                    continue
                targets.append(candidate)
    return sorted(set(targets))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--apply", action="store_true", help="Delete safe detected outputs")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if not (root / "CMakeLists.txt").is_file() or not (root / "Source/CMakeLists.txt").is_file():
        parser.error("root is not a SIREngine source tree")
    try:
        targets = plan_clean(root)
    except (OSError, subprocess.CalledProcessError) as exc:
        parser.error(f"cannot verify tracked file ownership: {exc}")
    for target in targets:
        print(f"{'REMOVE' if args.apply else 'WOULD REMOVE'} {target.relative_to(root)}")
        if args.apply:
            if target.is_dir():
                shutil.rmtree(target)
            else:
                target.unlink()
    print(f"{len(targets)} generated items {'removed' if args.apply else 'detected (dry-run)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

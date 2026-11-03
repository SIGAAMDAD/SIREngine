#!/usr/bin/env python3
"""Verify clean SIREngine tracked-file boundaries and dependency pinning."""
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
REQUIRED = (
    "CMakeLists.txt",
    "CMakePresets.json",
    ".gitmodules",
    ".clang-format",
    "docs/README.md",
    "docs/architecture/03-grdg.md",
    "include/sir/core/SIRContainers.h",
    ".github/workflows/ci.yml",
)
GITLINKS = (
    "engine/godot",
    "third_party/EASTL",
    "third_party/EABase",
    "third_party/EAAssert",
)
FORBIDDEN_SUFFIXES = (".obj", ".o", ".exe", ".dll", ".pdb", ".ilk", ".spv", ".log", ".pyc")
FORBIDDEN_ROOTS = ("out/", "build/", "CMakeFiles/", ".scons_cache/", ".vs/")


def tracked() -> list[str]:
    output = subprocess.check_output(("git", "ls-files", "-z"), cwd=ROOT)
    return [path.decode("utf-8") for path in output.split(b"\0") if path]


def main() -> int:
    paths = set(tracked())
    errors: list[str] = []

    for path in REQUIRED:
        if path not in paths:
            errors.append(f"Required tracked file missing: {path}")

    for path in sorted(paths):
        lowered = path.lower()
        if path.startswith(FORBIDDEN_ROOTS):
            errors.append(f"Generated directory tracked: {path}")
        if lowered.endswith(FORBIDDEN_SUFFIXES):
            errors.append(f"Build/log artifact tracked: {path}")
        if path in ("CMakeCache.txt", "Makefile", "cmake_install.cmake"):
            errors.append(f"Generated root file tracked: {path}")

    for path in GITLINKS:
        result = subprocess.check_output(
            ("git", "ls-files", "--stage", "--", path), cwd=ROOT, text=True
        )
        if not result.startswith("160000 "):
            errors.append(f"Dependency must be a pinned gitlink: {path}")

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(f"PASS: {len(paths)} tracked paths; no generated files; pinned gitlinks present.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

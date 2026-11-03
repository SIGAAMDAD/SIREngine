#!/usr/bin/env python3
"""Run the pinned formatter on SIREngine-owned C++ only."""
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PINNED_VERSION = "22.1.5"
OWNED = ("include/sir/", "src/", "tests/")
SUFFIXES = (".h", ".hpp", ".cpp", ".cc", ".c")


def main() -> int:
    binary = "clang-format"
    try:
        version = subprocess.check_output((binary, "--version"), text=True).strip()
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: pinned clang-format unavailable: {exc}", file=sys.stderr)
        return 1

    if PINNED_VERSION not in version:
        print(f"ERROR: expected clang-format {PINNED_VERSION}; got {version}", file=sys.stderr)
        return 1

    output = subprocess.check_output(("git", "ls-files", "-z"), cwd=ROOT)
    paths = [
        ROOT / entry.decode("utf-8")
        for entry in output.split(b"\0")
        if entry
        and entry.decode("utf-8").startswith(OWNED)
        and entry.decode("utf-8").endswith(SUFFIXES)
    ]

    if not paths:
        print("ERROR: no SIREngine-owned C++ files to validate", file=sys.stderr)
        return 1

    failed = []
    for path in paths:
        run = subprocess.run((binary, "--dry-run", "--Werror", str(path)), cwd=ROOT)
        if run.returncode != 0:
            failed.append(path.relative_to(ROOT).as_posix())

    if failed:
        print("ERROR: formatting failed: " + ", ".join(failed), file=sys.stderr)
        return 1

    print(f"PASS: {len(paths)} files formatted with clang-format {PINNED_VERSION}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

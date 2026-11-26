#!/usr/bin/env python3
/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

"""Portable staged-format and first-party policy pre-commit gate."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

from run_checks import SUFFIXES, compile_inventory, tool_version


def git(*args: str, cwd: Path, text: bool = False):
    return subprocess.run(["git", *args], cwd=cwd, check=True,
                          capture_output=True, text=text)


def main() -> int:
    try:
        root = Path(git("rev-parse", "--show-toplevel", cwd=Path.cwd(), text=True).stdout.strip()).resolve()
        paths = git("diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR",
                    "--", "Source", "Tests", "Tools", cwd=root).stdout
        paths = [Path(part.decode("utf-8", errors="surrogateescape")) for part in paths.split(b"\0") if part]
        paths = [path for path in paths if path.suffix.lower() in SUFFIXES and
                 len(path.parts) > 1 and path.parts[:2] != ("Tests", "Standards")]
        if not paths:
            return 0
        formatter = os.environ.get("SIR_CLANG_FORMAT") or shutil.which("clang-format-23") or shutil.which("clang-format")
        if not formatter:
            raise RuntimeError("clang-format 23.1.5 not found")
        tool_version(formatter, True)
        for path in paths:
            contents = git("show", ":" + path.as_posix(), cwd=root).stdout
            formatted = subprocess.run([formatter, "--style=file", "--assume-filename=" + str(root / path)],
                                       cwd=root, input=contents, capture_output=True, check=True).stdout
            if contents != formatted:
                print("STAGED FORMAT VIOLATION:", path, file=sys.stderr)
                return 1
        host = "Windows" if os.name == "nt" else ("Darwin" if sys.platform == "darwin" else "Linux")
        candidates = [root / "out" / host / preset / "compile_commands.json"
                      for preset in ("standards-clang23", "tooling")]
        selected = os.environ.get("SIR_COMPILE_COMMANDS")
        compdb = Path(selected).resolve() if selected else next((p for p in candidates if p.exists()), candidates[0])
        compile_inventory(root, compdb)
        runner = root / "Tools/Standards/run_checks.py"
        for mode in ("policy", "tidy"):
            cmd = [sys.executable, str(runner), "--repo", str(root), "--compdb", str(compdb), "--mode", mode]
            proc = subprocess.run(cmd, cwd=root)
            if proc.returncode:
                return proc.returncode
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print("STANDARDS PRE-COMMIT INFRASTRUCTURE FAILURE:", exc, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Compile owned headers independently using real CMake translation-unit flags.

Only headers with an owning translation unit in this configuration are tested.
Files in platform/optional subsystems not configured into the build are reported
as uncovered; they must be qualified by other configurations in the CI matrix.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from compdb import analysis_command, command_arguments
import subprocess
import sys

from run_checks import inventory, compile_inventory


def source_file(entry: dict) -> Path:
    p = Path(entry["file"])
    return (p if p.is_absolute() else Path(entry["directory"]) / p).resolve()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--clang", default="clang++-23")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    root = args.repo.resolve()
    compdb = args.compdb.resolve()
    try:
        files = compile_inventory(root, compdb)
        entries = json.loads(compdb.read_text())
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print("header check infrastructure failure:", exc, file=sys.stderr)
        return 2

    by_path = {source_file(e): e for e in entries if source_file(e) in files}
    headers = [p for p in inventory(root) if p.suffix.lower() in (".h", ".hpp", ".hxx")]
    errors = 0
    checked = 0
    uncovered = []
    for header in headers:
        # The first directory below Source/Tests/Tools is the subsystem root.
        relative = header.relative_to(root)
        group = relative.parts[:2] if len(relative.parts) > 2 else relative.parts[:1]
        peers = [p for p in files if (p.relative_to(root).parts[:2] if len(p.relative_to(root).parts) > 2
                                    else p.relative_to(root).parts[:1]) == group]
        if not peers:
            uncovered.append(str(relative))
            continue
        peer = max(peers, key=lambda p: sum(1 for a, b in zip(p.parent.parts, header.parent.parts) if a == b))
        entry = by_path[peer]
        args_original = command_arguments(entry)
        cmd = analysis_command(args.clang, args_original, peer, header=True)
        # Replace the selected header operand; leave native compiler flags intact.
        # clang-cl uses /TP /Zs instead of GCC's -x c++-header.
        cmd[-1] = str(header)
        try:
            proc = subprocess.run(cmd, cwd=entry["directory"], capture_output=True, text=True,
                                  timeout=args.timeout)
        except (OSError, subprocess.TimeoutExpired) as exc:
            print(f"HEADER ERROR {relative}: {exc}", file=sys.stderr)
            errors += 1
            continue
        checked += 1
        if proc.returncode:
            errors += 1
            print(f"HEADER ERROR {relative}: {proc.stderr[-1800:]}", file=sys.stderr)
    print(f"Header independence: checked={checked}, not-configured={len(uncovered)}, failures={errors}")
    for path in uncovered[:40]:
        print("NOT COVERED BY THIS CONFIGURATION:", path)
    if checked == 0:
        return 2
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Run SIREngine formatting, clang-tidy, and custom Clang AST policy gates.

Requires a real Ninja/FASTBuild-exported compilation database. No source edits.
Use --publish-compdb to copy the database to repository root for clangd.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--repo", type=Path, default=Path.cwd())
    p.add_argument("--compdb", type=Path, required=True)
    p.add_argument("--clang", default="clang++-23")
    p.add_argument("--clang-tidy", default="clang-tidy")
    p.add_argument("--clang-format", default="clang-format")
    p.add_argument("--publish-compdb", action="store_true")
    p.add_argument("--skip-ast", action="store_true")
    p.add_argument("--skip-tidy", action="store_true")
    p.add_argument("--skip-format", action="store_true")
    p.add_argument("--timeout", type=int, default=180)
    args = p.parse_args()
    root = args.repo.resolve()
    compdb = args.compdb.resolve()
    if args.publish_compdb:
        target = root / "compile_commands.json"
        if target != compdb:
            shutil.copyfile(compdb, target)
            print("clangd compilation database published:", target, flush=True)
    entries = json.loads(compdb.read_text())
    files = set()
    for entry in entries:
        directory = Path(entry["directory"])
        path = Path(entry["file"])
        if not path.is_absolute():
            path = directory / path
        path = path.resolve()
        try:
            rel = path.relative_to(root)
        except ValueError:
            continue
        if rel.parts and rel.parts[0] in ("Source", "Tests", "Tools") and not any(
            x in rel.parts for x in ("ThirdParty", "External", "Vendor", "out")
        ):
            files.add(path)
    print("First-party translation units:", len(files), flush=True)
    if not files:
        print("No first-party translation units: refusing false PASS", file=sys.stderr)
        return 2
    failed = []
    if not args.skip_ast:
        cmd = [sys.executable, str(Path(__file__).with_name("sir_clang_policy.py")), "--repo", str(root),
               "--compdb", str(compdb), "--clang", args.clang]
        result = subprocess.run(cmd, cwd=root)
        if result.returncode:
            failed.append(("SIREngine AST", result.returncode))
    for path in sorted(files):
        if not args.skip_format:
            cmd = [args.clang_format, "--style=file", str(path)]
            try:
                result = subprocess.run(cmd, cwd=root, timeout=args.timeout, capture_output=True)
                if result.returncode:
                    print(result.stderr.decode("utf-8", errors="replace"), file=sys.stderr)
                    failed.append(("format " + str(path), result.returncode))
                elif result.stdout != path.read_bytes():
                    print("clang-format differs:", path, file=sys.stderr)
                    failed.append(("format " + str(path), 1))
            except subprocess.TimeoutExpired:
                failed.append(("format timeout " + str(path), 124))
        if not args.skip_tidy:
            cmd = [args.clang_tidy, "-p", str(compdb.parent), "--config-file=" + str(root / ".clang-tidy"), str(path)]
            try:
                result = subprocess.run(cmd, cwd=root, timeout=args.timeout)
                if result.returncode:
                    failed.append(("tidy " + str(path), result.returncode))
            except subprocess.TimeoutExpired:
                failed.append(("tidy timeout " + str(path), 124))
    for label, code in failed:
        print("FAIL", code, label, file=sys.stderr)
    print("SIREngine Clang gates:", "FAIL" if failed else "PASS", "failed=", len(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

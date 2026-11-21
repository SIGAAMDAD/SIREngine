#!/usr/bin/env python3
"""SIREngine fail-closed format, clang-tidy, and Clang AST standards gates.

Use a genuine CMake/Ninja compilation database for tidy and policy analysis.
Formatting inspects all first-party C/C++ files, including headers and uncompiled
source; intentionally invalid standards fixtures are excluded from production gates.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

OWNED = ("Source", "Tests", "Tools")
EXCLUDED = {"ThirdParty", "External", "Vendor", "out", "build", ".git", "__pycache__"}
SUFFIXES = {".h", ".hpp", ".hxx", ".c", ".cc", ".cpp", ".cxx", ".inl"}


def owned(path: Path, root: Path) -> bool:
    try:
        relative = path.resolve().relative_to(root)
    except ValueError:
        return False
    return (len(relative.parts) >= 2 and relative.parts[0] in OWNED
            and not any(part in EXCLUDED for part in relative.parts)
            and relative.parts[:2] != ("Tests", "Standards"))


def inventory(root: Path) -> list[Path]:
    return sorted(path for directory in OWNED
                  for path in (root / directory).rglob("*")
                  if path.is_file() and path.suffix.lower() in SUFFIXES and owned(path, root))


def compile_inventory(root: Path, compdb: Path) -> list[Path]:
    if not compdb.is_file():
        raise ValueError("missing compilation database: " + str(compdb))
    entries = json.loads(compdb.read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not entries:
        raise ValueError("compilation database is empty")
    files = set()
    for entry in entries:
        base = Path(entry["directory"]).resolve()
        item = Path(entry["file"])
        path = (item if item.is_absolute() else base / item).resolve()
        if owned(path, root):
            if not path.is_file():
                raise ValueError("compiled source missing from disk: " + str(path))
            files.add(path)
    if not files:
        raise ValueError("compilation database has no first-party translation units")
    return sorted(files)


def tool_version(binary: str, must_be_pinned: bool) -> None:
    result = subprocess.run([binary, "--version"], capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise ValueError(binary + " --version failed")
    version = (result.stdout + result.stderr).strip()
    if must_be_pinned and not re.search(r"(?<!\d)23\.1\.5(?!\d)", version):
        raise ValueError("expected LLVM 23.1.5: " + binary + ": " + version[:180])
    print("tool:", binary, version.splitlines()[0], flush=True)


def execute(cmd: list[str], cwd: Path, timeout: int, quiet: bool = False) -> int:
    try:
        result = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        print("TIMEOUT:", " ".join(cmd[:5]), file=sys.stderr)
        return 124
    if result.returncode:
        print("FAILED:", " ".join(cmd[:5]), file=sys.stderr)
    if result.stdout and (result.returncode or not quiet):
        sys.stdout.buffer.write(result.stdout)
    if result.stderr:
        sys.stderr.buffer.write(result.stderr)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--compdb", type=Path)
    parser.add_argument("--mode", choices=("all", "format", "tidy", "policy", "regression", "publish-compdb"), default="all")
    parser.add_argument("--clang", default="clang++-23")
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--timeout", type=int, default=180)
    parser.add_argument("--no-version-pin", action="store_true", help="Only for local test fixtures")
    args = parser.parse_args()
    root = args.repo.resolve()
    compdb = args.compdb.resolve() if args.compdb else None
    if not root.is_dir():
        parser.error("repository does not exist")
    if args.timeout < 1:
        parser.error("timeout must be positive")
    try:
        needs_db = args.mode in ("all", "tidy", "policy", "publish-compdb")
        if needs_db and compdb is None:
            raise ValueError("--compdb is required for this gate")
        tu_files = compile_inventory(root, compdb) if needs_db else []
        if args.mode == "publish-compdb":
            target = root / "compile_commands.json"
            if target.resolve() != compdb:
                # Atomic replacement prevents clangd reading a half-written file.
                import os
                import tempfile
                with tempfile.NamedTemporaryFile(mode="wb", prefix=".sir-compdb-",
                                             suffix=".json", dir=root, delete=False) as temporary:
                    temporary.write(compdb.read_bytes())
                    tmp_name = Path(temporary.name)
                try:
                    os.replace(tmp_name, target)
                finally:
                    tmp_name.unlink(missing_ok=True)
            print("clangd compilation database:", target)
            return 0
        if args.mode in ("all", "format"):
            tool_version(args.clang_format, not args.no_version_pin)
        if args.mode in ("all", "tidy"):
            tool_version(args.clang_tidy, not args.no_version_pin)
        if args.mode in ("all", "policy", "regression"):
            tool_version(args.clang, not args.no_version_pin)
    except (OSError, ValueError, subprocess.TimeoutExpired, json.JSONDecodeError) as exc:
        print("STANDARDS INFRASTRUCTURE FAILURE:", str(exc), file=sys.stderr)
        return 2

    failures = []
    if args.mode in ("all", "format"):
        files = inventory(root)
        if not files:
            print("format inventory empty; refusing false PASS", file=sys.stderr)
            return 2
        print("Formatting first-party files:", len(files), flush=True)
        for path in files:
            # Byte-exact comparison catches all expected whitespace differences.
            try:
                formatted = subprocess.run([args.clang_format, "--style=file", str(path)], cwd=root,
                                           capture_output=True, timeout=args.timeout, check=True).stdout
                if formatted != path.read_bytes():
                    print("FORMAT VIOLATION:", path.relative_to(root), file=sys.stderr)
                    failures.append(("format", str(path)))
            except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                print("FORMAT INFRASTRUCTURE FAILURE:", path, exc, file=sys.stderr)
                failures.append(("format-parse", str(path)))
    if args.mode in ("all", "tidy"):
        print("clang-tidy translation units:", len(tu_files), flush=True)
        for path in tu_files:
            cmd = [args.clang_tidy, "-p", str(compdb.parent), "--config-file=" + str(root / ".clang-tidy"), str(path)]
            if execute(cmd, root, args.timeout, quiet=True):
                failures.append(("tidy", str(path)))
    if args.mode in ("all", "policy"):
        print("Clang AST policy compilation entries:", len(tu_files), flush=True)
        command = [sys.executable, str(root / "Tools/Standards/sir_clang_policy.py"),
                   "--repo", str(root), "--compdb", str(compdb), "--clang", args.clang,
                   "--timeout", str(args.timeout)]
        if execute(command, root, max(180, args.timeout * len(tu_files)), quiet=True):
            failures.append(("AST-policy", "full compilation database"))
    if args.mode == "regression":
        cmd = [sys.executable, str(root / "Tests/Standards/test_policy.py"), args.clang]
        if execute(cmd, root, args.timeout * 4):
            failures.append(("policy-regression", "C++17 and C++23"))
        cmd = [sys.executable, str(root / "Tests/Standards/test_runner.py"), args.clang_format]
        if execute(cmd, root, args.timeout * 2):
            failures.append(("runner-regression", "fail-closed behavior"))
        cmd = [sys.executable, str(root / "Tests/Standards/test_crossplatform.py"), args.clang]
        if execute(cmd, root, args.timeout * 2):
            failures.append(("crossplatform-regression", "Windows/Linux compile-command parsing"))
    for category, path in failures:
        print("FAIL", category, path, file=sys.stderr)
    print("SIREngine standards:", "FAIL" if failures else "PASS", "mode=" + args.mode,
          "failures=" + str(len(failures)), flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

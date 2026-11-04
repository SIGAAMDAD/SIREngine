#!/usr/bin/env python3
"""Generate verifiable Clang 23 ASTs from authentic input compilation commands."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--compdb", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    prefix = root / "Source" / "Core" / "Platform" / "Input"
    entries = json.loads(args.compdb.read_text())
    targets = []
    for entry in entries:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        source = source.resolve()
        if source.suffix == ".cpp" and (source.parent == prefix or prefix in source.parents):
            targets.append((source, entry))
    if not targets:
        print("BLOCKED_AST: no input translation units in authentic compile_commands.json")
        return 3
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    for source, entry in sorted(targets, key=lambda pair: str(pair[0])):
        tokens = entry.get("arguments") or shlex.split(entry["command"])
        executable = Path(tokens[0]).name
        if executable != "clang++-23":
            raise RuntimeError(f"not an authentic Clang 23 compilation: {executable}")
        cmd = []
        idx = 0
        remove_value = {"-o", "-MF", "-MT", "-MQ"}
        remove_one = {"-c", "-MD", "-MMD", "-MP", "-fsyntax-only"}
        while idx < len(tokens):
            token = tokens[idx]
            if token in remove_value:
                idx += 2
                continue
            if token in remove_one:
                idx += 1
                continue
            if token.startswith("@") or token.startswith("-Xclang") or token.startswith("-fplugin"):
                raise RuntimeError(f"unreviewed frontend response/plugin flag: {token}")
            cmd.append(token)
            idx += 1
        cmd.extend(["-H", "-Xclang", "-ast-dump=json", "-fsyntax-only"])
        relative = str(source.relative_to(root))
        stem = hashlib.sha256(relative.encode()).hexdigest()[:16]
        dump = args.out / f"{stem}.ast.json.gz"
        diagnostics = args.out / f"{stem}.stderr.txt"
        source_hash = sha(source.read_bytes())
        item = {"source": relative, "source_sha256": source_hash, "command_sha256": sha("\0".join(cmd).encode()),
                "compiler": executable, "ast": dump.name, "diagnostics": diagnostics.name}
        try:
            proc = subprocess.run(cmd, cwd=entry["directory"], capture_output=True, timeout=160, check=False)
            diagnostics.write_bytes(proc.stderr)
            item["exit_code"] = proc.returncode
            item["ast_bytes"] = len(proc.stdout)
            if proc.returncode == 0 and b'"kind": "TranslationUnitDecl"' in proc.stdout[:65536]:
                dump.write_bytes(gzip.compress(proc.stdout, compresslevel=3))
                item["ast_sha256"] = sha(dump.read_bytes())
                item["status"] = "AST_PASS"
            else:
                item["status"] = "BLOCKED_AST"
        except subprocess.TimeoutExpired as err:
            diagnostics.write_text(str(err))
            item["exit_code"] = None
            item["status"] = "BLOCKED_AST"
        results.append(item)
        print(item["status"], relative, item.get("ast_bytes", 0), flush=True)
    manifest = {"compiler": "clang++-23", "compdb_sha256": sha(args.compdb.read_bytes()),
                "input_tu_count": len(targets), "passed": sum(x["status"] == "AST_PASS" for x in results),
                "status": "AST_PASS" if all(x["status"] == "AST_PASS" for x in results) else "PARTIAL_AST_COVERAGE",
                "linux_native_only": True, "windows_native_not_run": True, "entries": results}
    (args.out / "ast-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f'AST result: {manifest["status"]} {manifest["passed"]}/{len(targets)}')
    return 0 if manifest["status"] == "AST_PASS" else 3


if __name__ == "__main__":
    sys.exit(main())

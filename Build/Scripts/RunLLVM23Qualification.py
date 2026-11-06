#!/usr/bin/env python3
"""Exact-SHA, full-compiled-production LLVM 23 source qualification."""
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / "out/Linux/llvm23-qualification"
REPORT = ROOT / "out/Verification/LLVM23"

def run(*args, env=None, stdout=None):
    return subprocess.run([str(a) for a in args], cwd=ROOT, env=env,
                          stdout=stdout, check=True, text=True)


def verify_production_coverage(required, files, source_root=None):
    """Qualify every mapped production implementation, including inline headers."""
    if not required:
        raise RuntimeError("Empty compiled production source inventory")
    root = pathlib.Path(source_root).resolve() if source_root is not None else None
    mapped = {}
    for item in files:
        filename = pathlib.Path(item["filename"]).resolve()
        if root is not None and root not in filename.parents:
            continue
        if filename.suffix not in (".c", ".cc", ".cpp", ".h", ".hpp", ".inl", ".inc"):
            continue
        key = str(filename)
        if key in mapped:
            raise RuntimeError(f"Duplicated production coverage mapping: {key}")
        mapped[key] = item
    missing = required - mapped.keys()
    if missing:
        raise RuntimeError(f"Missing compiled production coverage mappings: {sorted(missing)[:10]}")
    if not mapped:
        raise RuntimeError("No production coverage mappings")
    totals = {}
    failures = []
    for metric in ("lines", "branches", "functions", "regions"):
        try:
            counts = [item["summary"][metric] for item in mapped.values()]
            total = sum(row["count"] for row in counts)
            covered = sum(row["covered"] for row in counts)
        except (KeyError, TypeError) as exc:
            raise RuntimeError(f"Malformed LLVM {metric} coverage counters") from exc
        if total <= 0 or covered < 0 or covered > total or covered != total:
            failures.append(f"{metric}={covered}/{total}")
        totals[metric] = (covered, total)
    if failures:
        raise RuntimeError("100% production coverage unmet: " + ", ".join(failures))
    return totals



def find_zero_count_functions(coverage, source_root, limit=30):
    """List unexecuted mapped functions in the declared production scope."""
    root = pathlib.Path(source_root).resolve()
    matches = []
    for function in coverage.get("functions", []):
        if function.get("count") != 0:
            continue
        for source in function.get("filenames", []):
            path = pathlib.Path(source).resolve()
            if root not in path.parents:
                continue
            matches.append({"file": str(path), "name": function.get("name", ""), "count": 0})
            break
    return sorted(matches, key=lambda row: (row["file"], row["name"]))[:limit]


def qualify():
    for name in ("clang++-23", "llvm-cov-23", "llvm-profdata-23", "ninja"):
        if not shutil.which(name):
            raise RuntimeError(f"Missing required LLVM 23 tool: {name}")
    for name in ("clang++-23", "llvm-cov-23"):
        if "23.1.5" not in subprocess.check_output([name, "--version"], text=True):
            raise RuntimeError(f"Toolchain mismatch: {name} must be 23.1.5")
    REPORT.mkdir(parents=True, exist_ok=True)
    raw = REPORT / "raw"
    raw.mkdir(exist_ok=True)
    for path in raw.glob("*.profraw"):
        path.unlink()
    run("cmake", "--preset", "coverage-llvm23")
    if not (BUILD / "compile_commands.json").is_file():
        raise RuntimeError("LLVM qualification compiler inventory was not generated")
    run("cmake", "--build", BUILD, "--parallel", "3")
    env = dict(os.environ, LLVM_PROFILE_FILE=str(raw / "%p-%m.profraw"))
    run(sys.executable, ROOT / "Build/CI/RunCTestGate.py", "--test-dir", BUILD,
        "--report", REPORT / "ctest.xml", env=env)
    profiles = sorted(raw.glob("*.profraw"))
    if not profiles:
        raise RuntimeError("No execution profiles generated")
    profdata = REPORT / "merged.profdata"
    profile_merge = subprocess.run(["llvm-profdata-23", "merge", "-sparse",
                                    *map(str, profiles), "-o", str(profdata)],
                                   cwd=ROOT, capture_output=True, text=True,
                                   check=False)
    if profile_merge.returncode or "warning:" in profile_merge.stderr.lower():
        raise RuntimeError(f"Invalid LLVM profile merge: {profile_merge.stderr[:2000]}")
    objects = []
    for path in sorted(BUILD.rglob("*")):
        if path.is_file() and (path.stat().st_mode & 0o111):
            with path.open("rb") as handle:
                if handle.read(4) == b"\x7fELF":
                    objects.append(path)
    for path in sorted(BUILD.rglob("*.so")):
        if path.is_file() and path not in objects:
            objects.append(path)
    if not objects:
        raise RuntimeError("No instrumented binaries")
    out = REPORT / "llvm-export.json"
    command = ["llvm-cov-23", "export", str(objects[0]),
               *(f"-object={path}" for path in objects[1:]),
               f"-instr-profile={profdata}", "-format=text"]
    with out.open("w") as handle:
        result = subprocess.run(command, cwd=ROOT, stdout=handle,
                                stderr=subprocess.PIPE, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"LLVM export failed: {result.stderr[:2000]}")
    data = json.loads(out.read_text())
    if len(data.get("data", [])) != 1:
        raise RuntimeError("Invalid LLVM export")
    name = os.environ.get("SIR_QUALIFICATION_SCOPE", "all")
    scope_file = REPORT / "source-scope.txt"
    run("cmake", f"-DSIR_QUALIFICATION_SYSTEM={name}",
        f"-DSIR_QUALIFICATION_WRITE_PATH={scope_file}",
        "-P", ROOT / "Build/CMake/SIRQualificationScopes.cmake")
    source_root = (ROOT / scope_file.read_text().strip()).resolve()
    if any(word in result.stderr.lower() for word in ("mismatch", "out of date", "warning:")):
        candidates = find_zero_count_functions(data["data"][0], source_root)
        diagnostics = {"tool": "llvm-cov-23", "source_scope": str(source_root),
                       "stderr": result.stderr[:8000],
                       "zero_count_production_functions": candidates}
        (REPORT / "profile-mismatch.json").write_text(json.dumps(diagnostics, indent=2) + "\\n")
        raise RuntimeError(
            f"Coverage mapping/profile mismatch: {result.stderr[:2000]}; "
            f"first zero-count production functions: {candidates[:5]}; "
            f"see {REPORT / 'profile-mismatch.json'}")
    required = {str(pathlib.Path(row["file"]).resolve()) for row in
                json.loads((BUILD / "compile_commands.json").read_text())
                if pathlib.Path(row["file"]).suffix in (".cpp", ".cc", ".c")
                and source_root in pathlib.Path(row["file"]).resolve().parents}
    if not required:
        raise RuntimeError("Empty compiled production source inventory")
    totals = verify_production_coverage(required, data["data"][0]["files"], source_root)
    for name, (covered, count) in totals.items():
        print(f"LLVM 23 {name}: {covered}/{count}")
    print("LLVM 23 qualification PASS")

if __name__ == "__main__":
    try:
        qualify()
    except (RuntimeError, OSError, subprocess.CalledProcessError, ValueError, KeyError) as exc:
        print(f"LLVM qualification FAIL: {exc}", file=sys.stderr)
        sys.exit(1)

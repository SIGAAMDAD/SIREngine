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
    run("cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
        "-DCMAKE_CXX_COMPILER=clang++-23", "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_CXX_FLAGS=-fprofile-instr-generate -fcoverage-mapping -fno-omit-frame-pointer",
        "-DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate",
        "-DCMAKE_SHARED_LINKER_FLAGS=-fprofile-instr-generate",
        "-DSIR_BUILD_TESTS=ON", "-DSIR_WITH_GOOGLETEST=ON")
    run("cmake", "--build", BUILD, "--parallel", "3")
    env = dict(os.environ, LLVM_PROFILE_FILE=str(raw / "%p-%m.profraw"))
    run("ctest", "--test-dir", BUILD, "--output-on-failure", "--no-tests=error",
        "--output-junit", REPORT / "ctest.xml", env=env)
    profiles = sorted(raw.glob("*.profraw"))
    if not profiles:
        raise RuntimeError("No execution profiles generated")
    profdata = REPORT / "merged.profdata"
    run("llvm-profdata-23", "merge", "-sparse", *profiles, "-o", profdata)
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
    if result.returncode or "mismatch" in result.stderr.lower() or "out of date" in result.stderr.lower():
        raise RuntimeError(f"Coverage mapping/profile mismatch: {result.stderr[:2000]}")
    data = json.loads(out.read_text())
    if len(data.get("data", [])) != 1:
        raise RuntimeError("Invalid LLVM export")
    name = os.environ.get("SIR_QUALIFICATION_SCOPE", "all")
    roots = {"all": "Source", "GraphicsABI": "Source/Renderer"}
    roots.update({system: "Source/" + system for system in
                  ("Core", "Resources", "Renderer", "Terrain", "Editor", "Scripting", "Audio")})
    if name not in roots:
        raise RuntimeError("Unknown qualification scope")
    source_root = (ROOT / roots[name]).resolve()
    required = {str(pathlib.Path(row["file"]).resolve()) for row in
                json.loads((BUILD / "compile_commands.json").read_text())
                if pathlib.Path(row["file"]).suffix in (".cpp", ".cc", ".c")
                and source_root in pathlib.Path(row["file"]).resolve().parents}
    if not required:
        raise RuntimeError("Empty compiled production source inventory")
    mapped = {str(pathlib.Path(item["filename"]).resolve()): item
              for item in data["data"][0]["files"]}
    missing = required - mapped.keys()
    if missing:
        raise RuntimeError(f"Missing compiled production coverage mappings: {sorted(missing)[:10]}")
    print(f"Production compiled translation units: {len(required)}")
    for metric in ("lines", "branches", "functions", "regions"):
        values = [mapped[path]["summary"][metric] for path in required]
        count = sum(row["count"] for row in values)
        covered = sum(row["covered"] for row in values)
        print(f"LLVM 23 {metric}: {covered}/{count}")
        if count == 0 or covered != count:
            raise RuntimeError(f"Unmet 100% {metric} coverage requirement")
    print("LLVM 23 qualification PASS")

if __name__ == "__main__":
    try:
        qualify()
    except (RuntimeError, OSError, subprocess.CalledProcessError, ValueError, KeyError) as exc:
        print(f"LLVM qualification FAIL: {exc}", file=sys.stderr)
        sys.exit(1)

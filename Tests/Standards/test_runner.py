#!/usr/bin/env python3
"""Fail-closed checks for standards runner infrastructure and formatting."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "Tools/Standards/run_checks.py"
FORMATTER = sys.argv[1] if len(sys.argv) > 1 else "clang-format"


def invoke(*args):
    return subprocess.run([sys.executable, str(SCRIPT), *args], capture_output=True, text=True)


with tempfile.TemporaryDirectory(prefix="sir-gate-test-") as location:
    repo = Path(location)
    (repo / "Source").mkdir()
    (repo / ".clang-format").write_bytes((ROOT / ".clang-format").read_bytes())
    file = repo / "Source/SIRExample.cpp"
    file.write_text("void Test( void )\n{\n\treturn;\n}\n")
    formatted = subprocess.run([FORMATTER, "--style=file", str(file)], check=True,
                               capture_output=True).stdout
    file.write_bytes(formatted)
    result = invoke("--repo", str(repo), "--mode", "format", "--clang-format", FORMATTER)
    assert result.returncode == 0, (result.stdout, result.stderr)

    file.write_text("void Test(void){return;}\n")
    result = invoke("--repo", str(repo), "--mode", "format", "--clang-format", FORMATTER)
    assert result.returncode == 1, (result.stdout, result.stderr)
    assert "FORMAT VIOLATION" in result.stderr

    result = invoke("--repo", str(repo), "--mode", "policy", "--compdb", str(repo / "missing.json"))
    assert result.returncode == 2, (result.stdout, result.stderr)

    (repo / "empty.json").write_text("[]\n")
    result = invoke("--repo", str(repo), "--mode", "tidy", "--compdb", str(repo / "empty.json"))
    assert result.returncode == 2, (result.stdout, result.stderr)

    file.unlink()
    result = invoke("--repo", str(repo), "--mode", "format", "--clang-format", FORMATTER)
    assert result.returncode == 2, (result.stdout, result.stderr)

print("Standards runner failure-path fixtures PASS")

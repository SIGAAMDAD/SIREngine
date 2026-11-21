#!/usr/bin/env python3
"""Regression tests: the good fixture passes, the bad fixture fails exact AST rules."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
script = root / "Tools/Standards/sir_clang_policy.py"
clang = sys.argv[1] if len(sys.argv) > 1 else "clang++-23"
for standard in ("c++17", "c++23"):
    for case, passing in (("positive", True), ("negative", False)):
        result = subprocess.run([sys.executable, str(script), "--repo", str(root), "--clang", clang,
                                 "--file", str(Path(__file__).parent / (case + ".cpp")), "--std", standard,
                                 "--json", str(Path(__file__).parent / (case + "_" + standard + ".json"))],
                                text=True, capture_output=True)
        expected = 0 if passing else 1
        assert result.returncode == expected, (case, standard, result.stdout, result.stderr)
        report = json.loads((Path(__file__).parent / (case + "_" + standard + ".json")).read_text())
        rules = {entry["rule"] for entry in report["diagnostics"]}
        if not passing:
            assert {"SIR001", "SIR002", "SIR010", "SIR014", "SIR017", "SIR020", "SIR021", "SIR024", "SIR025", "SIR003", "SIR008", "SIR013", "SIR030", "SIR037"} <= rules, rules
        else:
            assert not rules and report["checked_tus"] == 1, report
        print(standard, case, report["status"], "diagnostics=", len(rules))
print("Policy fixture tests PASS")

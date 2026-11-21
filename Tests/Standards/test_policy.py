#!/usr/bin/env python3
"""C++17/C++23 positive/negative regression for every implemented SIR policy rule."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
policy = root / "Tools/Standards/sir_clang_policy.py"
clang = sys.argv[1] if len(sys.argv) > 1 else "clang++-23"
expected_negative = {"SIR001", "SIR002", "SIR003", "SIR007", "SIR008", "SIR010",
                     "SIR011", "SIR013", "SIR014", "SIR017", "SIR018", "SIR020",
                     "SIR021", "SIR024", "SIR025", "SIR030", "SIR032", "SIR034", "SIR037"}
expected_extra = {"SIR004", "SIR005", "SIR006", "SIR007", "SIR009", "SIR010",
                  "SIR011", "SIR012", "SIR013", "SIR015", "SIR016", "SIR017",
                  "SIR018", "SIR019", "SIR022", "SIR023", "SIR024", "SIR026",
                  "SIR027", "SIR028", "SIR029", "SIR031", "SIR032", "SIR033", "SIR035"}
covered = set()
with tempfile.TemporaryDirectory(prefix="sir-policy-fixtures-") as directory:
    for standard in ("c++17", "c++23"):
        for name, expected in (("positive", set()), ("negative", expected_negative),
                               ("negative_extra", expected_extra)):
            report_file = Path(directory) / (name + "_" + standard + ".json")
            args = [sys.executable, str(policy), "--repo", str(root), "--clang", clang,
                    "--file", str(Path(__file__).parent / (name + ".cpp")),
                    "--std", standard, "--json", str(report_file)]
            result = subprocess.run(args, capture_output=True, text=True)
            expected_return = 0 if name == "positive" else 1
            assert result.returncode == expected_return, (name, standard, result.stdout, result.stderr)
            report = json.loads(report_file.read_text())
            rules = {entry["rule"] for entry in report["diagnostics"]}
            assert expected <= rules, (name, standard, sorted(expected - rules))
            assert report["checked_tus"] == 1 and not report["failed_tus"], report
            if name == "positive":
                assert not rules, report
            else:
                covered |= rules
            print(standard, name, report["status"], "rules=", len(rules))

header_test = subprocess.run([sys.executable, str(Path(__file__).parent / "test_headers.py"), clang],
                             capture_output=True, text=True)
assert header_test.returncode == 0, (header_test.stdout, header_test.stderr)
print(header_test.stdout.strip())
covered.update({"SIR038", "SIR040", "SIR041"})
implemented = set(re.findall(r"SIR\d{3}", policy.read_text()))
assert implemented <= covered, "Uncovered policy rule fixtures: " + str(sorted(implemented - covered))
print("Policy fixture tests PASS: ", len(implemented), "rule identifiers covered")

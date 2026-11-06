#!/usr/bin/env python3
"""Run a CTest selection and reject empty, omitted, failed, or skipped tests."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET


def verify_results(expected_names, report_path, exit_code):
    expected = list(expected_names)
    if not expected or len(expected) != len(set(expected)):
        raise RuntimeError("Required CTest selection is empty or contains duplicate names")
    root = ET.parse(report_path).getroot()
    cases = list(root.iter("testcase"))
    actual = [case.get("name") for case in cases]
    if len(actual) != len(set(actual)):
        raise RuntimeError("CTest JUnit contains duplicate test names")
    missing = set(expected) - set(actual)
    extra = set(actual) - set(expected)
    if missing or extra:
        raise RuntimeError(f"CTest inventory mismatch: missing={sorted(missing)} extra={sorted(extra)}")
    skipped = [
        case.get("name") for case in cases
        if case.find("skipped") is not None or
        case.get("status", "").lower() in ("notrun", "skipped")
    ]
    failed = [
        case.get("name") for case in cases
        if case.find("failure") is not None or case.find("error") is not None
    ]
    if exit_code != 0 or skipped or failed:
        raise RuntimeError(f"CTest did not fully pass: exit={exit_code}, "
                           f"skipped={skipped}, failed={failed}")
    return len(actual)


def run_gate(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    selector = parser.add_mutually_exclusive_group(required=True)
    selector.add_argument("--preset")
    selector.add_argument("--test-dir", type=Path)
    parser.add_argument("--label", help="CTest -L expression, omitted for full suite")
    parser.add_argument("--name-regex", help="CTest -R expression, omitted for all names")
    parser.add_argument("--repeat", type=int, default=1, help="Nightly-only repetition count")
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args(argv)
    base = ["ctest", "--preset", args.preset] if args.preset else [
        "ctest", "--test-dir", str(args.test_dir)]
    if args.label:
        base += ["-L", args.label]
    if args.name_regex:
        base += ["-R", args.name_regex]
    if args.repeat < 1 or args.repeat > 20:
        raise RuntimeError("Invalid CTest repeat count")
    enumeration = subprocess.run(
        base + ["--show-only=json-v1"], capture_output=True, text=True, check=False)
    if enumeration.returncode != 0:
        raise RuntimeError(f"CTest inventory discovery failed: {enumeration.stderr}")
    try:
        selection = json.loads(enumeration.stdout)
        expected = [case["name"] for case in selection["tests"]]
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError("Malformed CTest inventory output") from exc
    if not expected:
        raise RuntimeError("CTest selection matched zero required tests")
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.unlink(missing_ok=True)
    command = base + ["--output-on-failure", "--no-tests=error", "--output-junit",
                      str(args.report)]
    if args.repeat > 1:
        command += ["--repeat", f"until-fail:{args.repeat}"]
    execution = subprocess.run(command, check=False)
    if not args.report.is_file():
        raise RuntimeError("CTest did not produce the required JUnit report")
    count = verify_results(expected, args.report, execution.returncode)
    print(f"CTest evidence PASS: {count} executed, zero failed or skipped")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(run_gate())
    except (RuntimeError, OSError, ET.ParseError) as exc:
        print(f"CTest evidence FAIL: {exc}", file=sys.stderr)
        sys.exit(1)

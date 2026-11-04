#!/usr/bin/env python3
"""Summarize GCC/gcov line and branch coverage for integrated GRDG/Core sources."""

import argparse
import gzip
import json
import subprocess
import sys
import tempfile
from pathlib import Path

TARGETS = (
    "Source/Resources/SIRResourceGraph.cpp",
    "Source/Resources/SIRResourceFileLoad.cpp",
    "Source/Resources/SIRResourceDerivedRegistry.cpp",
    "Source/Core/Jobs/SIRTaskScheduler.cpp",
    "Source/Core/Jobs/SIRTaskGraph.cpp",
    "Source/Core/Jobs/SIRTaskFenceTracker.cpp",
    "Source/Core/Memory/SIRMemoryManager.cpp",
    "Source/Core/Platform/Linux/SIRPlatformFileLinux.cpp",
    "Source/Core/Platform/Linux/SIRPlatformFileMappingLinux.cpp",
)


def summary(lines):
    executable = [entry for entry in lines if isinstance(entry.get("count"), (int, float))]
    covered = sum(entry["count"] > 0 for entry in executable)
    branches = [branch for entry in executable for branch in entry.get("branches", [])]
    branch_covered = sum(entry.get("count", 0) > 0 for entry in branches)
    return {
        "executable_lines": len(executable),
        "covered_lines": covered,
        "line_percent": round(100.0 * covered / len(executable), 2) if executable else 0.0,
        "branches": len(branches),
        "covered_branches": branch_covered,
        "branch_percent": round(100.0 * branch_covered / len(branches), 2) if branches else None,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--min-line-percent", type=float, default=0.0)
    parser.add_argument("--min-branch-percent", type=float, default=0.0)
    args = parser.parse_args()
    if not (0 <= args.min_line_percent <= 100 and 0 <= args.min_branch_percent <= 100):
        parser.error("coverage thresholds must be percentages from 0 through 100")

    build = Path(args.build).resolve()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not build.is_dir():
        parser.error("build directory does not exist")

    result = {}
    missing = []
    with tempfile.TemporaryDirectory() as workdir:
        for source in TARGETS:
            source_file = Path(source).name
            candidates = [
                path for path in build.rglob(source_file + ".gcda")
                if "/CMakeFiles/" in path.as_posix()
            ]
            if len(candidates) != 1:
                missing.append(f"{source}: found {len(candidates)} .gcda objects")
                continue
            work = Path(workdir) / source_file
            work.mkdir(parents=True, exist_ok=True)
            command = subprocess.run(
                ["gcov", "--json-format", "--branch-probabilities", "--branch-counts", str(candidates[0])],
                cwd=work, capture_output=True, text=True, check=False
            )
            if command.returncode != 0:
                missing.append(f"{source}: gcov failed: {command.stderr.strip()[:300]}")
                continue
            archives = list(work.glob("*.gcov.json.gz"))
            matches = []
            for archive in archives:
                with gzip.open(archive, "rt", encoding="utf-8") as file:
                    data = json.load(file)
                matches.extend(
                    x for x in data.get("files", [])
                    if x.get("file", "").replace("\\", "/").endswith(source)
                )
            if len(matches) != 1:
                missing.append(f"{source}: found {len(matches)} matching gcov JSON entries")
                continue
            result[source] = summary(matches[0].get("lines", []))

    report = {
        "tool": "gcc gcov json",
        "status": "measured" if not missing else "incomplete",
        "sources": result,
        "missing": missing,
    }
    (output / "grdg-coverage.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# GRDG/Core C++ coverage (instrumented GCC)",
        "",
        "| Source | Executable lines | Covered lines | Line % | Branches | Covered branches | Branch % |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for source in TARGETS:
        if source not in result:
            continue
        entry = result[source]
        branch_value = f"{entry['branch_percent']:.2f}" if entry["branch_percent"] is not None else "n/a"
        lines.append(
            f"| {source} | {entry['executable_lines']} | {entry['covered_lines']} | "
            f"{entry['line_percent']:.2f} | {entry['branches']} | {entry['covered_branches']} | {branch_value} |"
        )
    if missing:
        lines.extend(("", "## Missing instrumented targets", *(f"- {item}" for item in missing)))
    lines.extend(("", "Compiler coverage is test execution evidence, not a memory-safety proof."))
    (output / "grdg-coverage.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    if missing:
        print("GRDG coverage collector FAILED: required object evidence missing", file=sys.stderr)
        return 1
    if any(x["executable_lines"] == 0 or x["covered_lines"] == 0 for x in result.values()):
        print("GRDG coverage collector FAILED: target has no executed statements", file=sys.stderr)
        return 1
    failures = []
    for source, measurement in result.items():
        if measurement["line_percent"] < args.min_line_percent:
            failures.append(f"{source}: {measurement['line_percent']:.2f}% lines below {args.min_line_percent:.2f}%")
        branch = measurement["branch_percent"]
        if branch is None or branch < args.min_branch_percent:
            failures.append(f"{source}: {branch}% branches below {args.min_branch_percent:.2f}%")
    if failures:
        print("Coverage acceptance FAILED: " + "; ".join(failures), file=sys.stderr)
        return 1
    print(f"GRDG coverage acceptance PASS: each TU >= {args.min_line_percent:.1f}% lines and "
          f">= {args.min_branch_percent:.1f}% branches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Collect exact-revision GCC/gcov coverage for the nine GRDG/Core production TUs."""

import argparse
import csv
import gzip
import hashlib
import json
import os
import re
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


# Keep raw gcov branch counts. In particular, a negative count is an
# unexecuted outcome, not a branch to drop from the detailed report.
def summary(lines):
    executable = [entry for entry in lines if isinstance(entry.get("count"), (int, float))]
    covered = sum(entry["count"] > 0 for entry in executable)
    branches = [branch for entry in executable for branch in entry.get("branches", [])]
    branch_covered = sum(isinstance(branch.get("count"), (int, float)) and branch["count"] > 0
                         for branch in branches)
    return {
        "executable_lines": len(executable),
        "covered_lines": covered,
        "line_percent": round(100.0 * covered / len(executable), 2) if executable else 0.0,
        "branches": len(branches),
        "covered_branches": branch_covered,
        "branch_percent": round(100.0 * branch_covered / len(branches), 2) if branches else None,
    }


def uncovered_details(source, source_lines, source_sha256):
    """Return line-scoped gcov diagnostics and an exhaustive uncovered counter index."""
    details = []
    index = []
    for entry in source_lines:
        if not isinstance(entry.get("count"), (int, float)):
            continue
        line = entry["line_number"]
        line_count = entry["count"]
        outcomes = []
        if line_count <= 0:
            index.append({
                "source": source, "line": line, "kind": "line", "ordinal": "",
                "count": line_count, "basic_block": "", "classification": "uncovered_line",
                "source_sha256": source_sha256,
            })
        for ordinal, branch in enumerate(entry.get("branches", [])):
            count = branch.get("count", 0)
            if not isinstance(count, (int, float)):
                raise ValueError(f"{source}:{line}: nonnumeric branch count {count!r}")
            if count > 0:
                continue
            classification = "negative_gcov_count" if count < 0 else "unexecuted_branch"
            outcome = {
                "ordinal": ordinal,
                "count": count,
                "basic_block": branch.get("blockno"),
                "throw": branch.get("throw", False),
                "fallthrough": branch.get("fallthrough", False),
                "classification": classification,
            }
            outcomes.append(outcome)
            index.append({
                "source": source, "line": line, "kind": "branch", "ordinal": ordinal,
                "count": count, "basic_block": branch.get("blockno", ""),
                "classification": classification, "source_sha256": source_sha256,
            })
        if line_count <= 0 or outcomes:
            details.append({
                "line": line,
                "count": line_count,
                "source_sha256": source_sha256,
                "branches": entry.get("branches", []),
                "uncovered_branches": outcomes,
            })
    return details, index


def source_revision(explicit):
    candidate = explicit or os.environ.get("GITHUB_SHA", "")
    if not candidate:
        process = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                                 text=True, check=False)
        if process.returncode == 0:
            candidate = process.stdout.strip()
    return candidate if re.fullmatch(r"[0-9a-fA-F]{40}", candidate) else None


def coverage_failures(result, min_line_percent, min_branch_percent, require_complete):
    """Use integer counters for 100% acceptance, never rounded percentages."""
    failures = []
    for source, measurement in result.items():
        if measurement["covered_lines"] * 100 < measurement["executable_lines"] * min_line_percent:
            failures.append(f"{source}: insufficient executable-line coverage")
        if measurement["covered_branches"] * 100 < measurement["branches"] * min_branch_percent:
            failures.append(f"{source}: insufficient branch coverage")
        if require_complete and (measurement["covered_lines"] != measurement["executable_lines"] or
                                 measurement["covered_branches"] != measurement["branches"]):
            failures.append(f"{source}: {measurement['executable_lines']-measurement['covered_lines']} uncovered lines, "
                            f"{measurement['branches']-measurement['covered_branches']} uncovered branches")
    return failures


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-root", default=str(Path(__file__).resolve().parents[2]))
    parser.add_argument("--source-commit", help="Exact SHA of the compiled repository revision")
    parser.add_argument("--min-line-percent", type=float, default=0.0)
    parser.add_argument("--min-branch-percent", type=float, default=0.0)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()
    if not (0 <= args.min_line_percent <= 100 and 0 <= args.min_branch_percent <= 100):
        parser.error("coverage thresholds must be percentages from 0 through 100")
    if args.source_commit and not re.fullmatch(r"[0-9a-fA-F]{40}", args.source_commit):
        parser.error("--source-commit must be an exact 40-character SHA")

    build = Path(args.build).resolve()
    source_root = Path(args.source_root).resolve()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not build.is_dir():
        parser.error("build directory does not exist")
    revision = source_revision(args.source_commit)

    result = {}
    details = {}
    index = []
    missing = []
    with tempfile.TemporaryDirectory() as workdir:
        for source in TARGETS:
            source_path = source_root / source
            if not source_path.is_file():
                missing.append(f"{source}: source does not exist in source-root {source_root}")
                continue
            digest = hashlib.sha256(source_path.read_bytes()).hexdigest()
            source_file = source_path.name
            candidates = [path for path in build.rglob(source_file + ".gcda")
                          if "/CMakeFiles/" in path.as_posix()]
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
            matches = []
            for archive in work.glob("*.gcov.json.gz"):
                with gzip.open(archive, "rt", encoding="utf-8") as file:
                    data = json.load(file)
                matches.extend(x for x in data.get("files", [])
                               if x.get("file", "").replace("\\", "/").endswith(source))
            if len(matches) != 1:
                missing.append(f"{source}: found {len(matches)} matching gcov JSON entries")
                continue
            source_lines = matches[0].get("lines", [])
            measurement = summary(source_lines)
            try:
                uncovered, indexed = uncovered_details(source, source_lines, digest)
            except ValueError as exc:
                missing.append(str(exc))
                continue
            if sum(row["kind"] == "line" for row in indexed) != (
                measurement["executable_lines"] - measurement["covered_lines"]
            ) or sum(row["kind"] == "branch" for row in indexed) != (
                measurement["branches"] - measurement["covered_branches"]
            ):
                missing.append(f"{source}: detailed counter index disagrees with summary")
                continue
            measurement["source_sha256"] = digest
            result[source] = measurement
            details[source] = uncovered
            index.extend(indexed)

    report = {
        "tool": "gcc gcov json",
        "status": "measured" if not missing else "incomplete",
        "source_commit": revision,
        "sources": result,
        "missing": missing,
    }
    (output / "grdg-coverage.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (output / "grdg-coverage-detail.json").write_text(json.dumps({
        "source_commit": revision,
        "uncovered": details,
        "uncovered_counter_count": len(index),
    }, indent=2) + "\n", encoding="utf-8")
    (output / "grdg-uncovered-index.json").write_text(json.dumps({
        "source_commit": revision,
        "counters": index,
    }, indent=2) + "\n", encoding="utf-8")
    with (output / "grdg-uncovered-index.csv").open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=(
            "source", "line", "kind", "ordinal", "count", "basic_block", "classification", "source_sha256"))
        writer.writeheader()
        writer.writerows(index)

    lines = [
        "# GRDG/Core C++ coverage (instrumented GCC)",
        "",
        f"Source commit: {revision or 'NOT_VERIFIED'}",
        "",
        "| Source | Executable lines | Covered lines | Line % | Branches | Covered branches | Branch % |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for source in TARGETS:
        if source not in result:
            continue
        entry = result[source]
        branch_value = f"{entry['branch_percent']:.2f}" if entry["branch_percent"] is not None else "n/a"
        lines.append(f"| {source} | {entry['executable_lines']} | {entry['covered_lines']} | "
                     f"{entry['line_percent']:.2f} | {entry['branches']} | {entry['covered_branches']} | {branch_value} |")
    if missing:
        lines.extend(("", "## Missing or inconsistent objects", *(f"- {item}" for item in missing)))
    lines.extend(("", "Compiler coverage is test execution evidence, not a memory-safety proof."))
    (output / "grdg-coverage.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))

    if missing:
        print("GRDG coverage collector FAILED: required object evidence missing", file=sys.stderr)
        return 1
    if any(x["executable_lines"] == 0 or x["covered_lines"] == 0 for x in result.values()):
        print("GRDG coverage collector FAILED: target has no executed statements", file=sys.stderr)
        return 1
    if args.require_complete and revision is None:
        print("GRDG coverage collector FAILED: complete gate requires a verified source commit", file=sys.stderr)
        return 1

    failures = coverage_failures(result, args.min_line_percent, args.min_branch_percent, args.require_complete)
    if failures:
        print("Coverage acceptance FAILED: " + "; ".join(failures), file=sys.stderr)
        return 1
    print(f"GRDG coverage acceptance PASS: each TU >= {args.min_line_percent:.1f}% lines and "
          f">= {args.min_branch_percent:.1f}% branches" + (", all exact counters covered" if args.require_complete else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

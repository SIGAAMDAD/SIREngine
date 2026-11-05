#!/usr/bin/env python3
"""Report uncovered renderer ABI LLVM source locations for targeted regression tests."""
import collections
import json
import pathlib
import sys

SOURCES = {
    "SIRGraphicsDriverABI.cpp",
    "SIRGraphicsModule.cpp",
    "SIRGraphicsPipelineABI.cpp",
    "SIRGraphicsPipelineBridge.cpp",
    "SIRGraphicsRegistry.cpp",
    "SIRGraphicsValidation.cpp",
}


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: ReportGraphicsABIGaps.py llvm-export.json repo-root report.json")
    source = pathlib.Path(sys.argv[1])
    repo = pathlib.Path(sys.argv[2]).resolve()
    destination = pathlib.Path(sys.argv[3])
    data = json.loads(source.read_text(encoding="utf-8"))
    report = {}
    for entry in data.get("data", []):
        for file in entry.get("files", []):
            filename = pathlib.Path(file["filename"]).resolve()
            if filename.name not in SOURCES or filename.parent != repo / "Source/Renderer":
                continue
            if filename.name in report:
                raise SystemExit("duplicate source mapping: " + filename.name)
            lines = collections.defaultdict(lambda: {"covered": False, "uncovered": False})
            for segment in file.get("segments", []):
                if len(segment) < 5 or not segment[3] or not segment[4]:
                    continue
                line = segment[0]
                if segment[2]:
                    lines[line]["covered"] = True
                else:
                    lines[line]["uncovered"] = True
            missing_line_candidates = sorted(line for line, state in lines.items() if state["uncovered"] and not state["covered"])
            missing_branches = []
            for branch in file.get("branches", []):
                if len(branch) < 6:
                    continue
                if branch[4] == 0 or branch[5] == 0:
                    missing_branches.append({
                        "line": branch[0],
                        "column": branch[1],
                        "true": branch[4],
                        "false": branch[5],
                    })
            counts = collections.Counter(item["line"] for item in missing_branches)
            report[filename.name] = {
                "summary": file.get("summary", {}),
                "uncovered_region_entry_lines": missing_line_candidates,
                "uncovered_branch_outcomes": missing_branches,
                "missed_branch_lines_ranked": [
                    {"line": line, "outcomes": count}
                    for line, count in counts.most_common()
                ],
            }
    if set(report) != SOURCES:
        raise SystemExit("missing source mappings in LLVM export: " + ", ".join(sorted(SOURCES - set(report))))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for name in sorted(report):
        info = report[name]
        summary = info["summary"]
        print(
            f"GAPS {name}: "
            f"lines {summary['lines']['count'] - summary['lines']['covered']} "
            f"branches {summary['branches']['count'] - summary['branches']['covered']}"
        )
        print("  Zero-count region starts: " + json.dumps(info["uncovered_region_entry_lines"]))
        print("  Uncovered branch sites: " + json.dumps(info["missed_branch_lines_ranked"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

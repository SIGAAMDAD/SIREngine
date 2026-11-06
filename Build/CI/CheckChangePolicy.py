#!/usr/bin/env python3
"""Fail closed on invalid SIREngine commit subjects and PR admission metadata."""
import argparse
import os
from pathlib import Path
import re
import sys

HEADER = re.compile(
    r"^(?P<kind>feat|fix|refactor|perf|test|docs|style|build|ci|chore|revert)"
    r"(?:\((?P<scope>[a-z][a-z0-9-]*(?:/[a-z0-9-]+)*)\))?"
    r"(?P<breaking>!)?: (?P<subject>\S.*)$"
)
ISSUE = re.compile(
    r"(?<![A-Za-z0-9_])(?:\#[1-9][0-9]*|"
    r"https://github\.com/SIGAAMDAD/SIREngine/issues/[1-9][0-9]*)(?![0-9])"
)


def validate_commit(message):
    lines = message.splitlines()
    heading = lines[0] if lines else ""
    errors = []
    match = HEADER.fullmatch(heading)
    if not match:
        errors.append("Use a Conventional Commit subject: type(scope): description")
    if len(heading) > 72:
        errors.append("Commit subject exceeds 72 characters")
    if heading.endswith("."):
        errors.append("Commit subject must not end in a period")
    if match and match.group("breaking") and not re.search(
            r"(?m)^BREAKING CHANGE: \S", message):
        errors.append("Breaking commits require a BREAKING CHANGE: footer")
    return errors


def validate_pr(title, body):
    errors = validate_commit(title)
    if "\n" in title or "\r" in title:
        errors.append("PR title must be one line")
    if not ISSUE.search(body or ""):
        errors.append("PR body must reference an authoritative SIREngine issue (#N)")
    return errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--commit-file", type=Path)
    group.add_argument("--pr-from-env", action="store_true")
    args = parser.parse_args(argv)
    if args.commit_file is not None:
        problems = validate_commit(args.commit_file.read_text(encoding="utf-8-sig"))
    else:
        problems = validate_pr(os.environ.get("SIR_PR_TITLE", ""),
                               os.environ.get("SIR_PR_BODY", ""))
    for problem in problems:
        print("SIREngine change policy: " + problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Idempotently define SIREngine project-management labels; never apply approval labels."""
from __future__ import annotations

import argparse
import json
import os
import sys
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen

REPO = "SIGAAMDAD/SIREngine"
LABELS = {
    "marathon:tracked": ("6B7280", "Add issue or PR to SIREngine project intake"),
    "marathon:approved": ("1D4ED8", "Owner-approved exact scope; approval receipt must be verified"),
    "marathon:ready": ("16A34A", "Dependencies cleared; owner scope still needs independent verification"),
    "marathon:blocked": ("B91C1C", "Blocked by an explicit dependency, owner decision or verification failure"),
    "marathon:active": ("D97706", "Implementation in progress; coordination claim is authoritative"),
    "marathon:merge-approved": ("7C3AED", "Exact PR head/base integration approval receipt required"),
    "marathon:manual-stage": ("475569", "Human-managed Delivery Stage; automated sync will preserve it"),
    "kind:roadmap": ("334155", "Parent subsystem roadmap"),
    "kind:milestone": ("475569", "Approved milestone container or proposed milestone"),
    "kind:implementation": ("2563EB", "Bounded source implementation task"),
    "kind:bug": ("DC2626", "Defect with reproduction and acceptance"),
    "kind:verification": ("14B8A6", "Independent exact-SHA QA or benchmark task"),
    "kind:documentation": ("94A3B8", "Documentation and contributor work"),
    "priority:p0": ("991B1B", "Highest owner-set priority"),
    "priority:p1": ("C2410C", "High priority"),
    "priority:p2": ("CA8A04", "Normal priority"),
    "priority:p3": ("64748B", "Low priority"),
    "era:fixed": ("A16207", "Fixed-function graphics pipeline"),
    "era:programmable": ("0369A1", "Programmable graphics pipeline"),
    "era:modern": ("7C3AED", "Modern graphics pipeline"),
    "era:cross-era": ("0F766E", "Shared across graphics eras"),
}
for slug in (
    "core", "renderer", "grdg", "shaders", "opengl", "directx", "vulkan",
    "meshing", "terrain", "physics", "audio", "input", "scripting", "console",
    "editor", "world", "networking", "qa",
):
    LABELS["subsystem:" + slug] = ("0E7490", "SIREngine workstream: " + slug)


def request(method: str, path: str, token: str, body=None):
    url = "https://api.github.com/" + path.lstrip("/")
    data = json.dumps(body).encode() if body is not None else None
    req = Request(url, data=data, method=method, headers={
        "Authorization": "Bearer " + token,
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "SIREngine-label-bootstrap",
    })
    try:
        with urlopen(req, timeout=30) as result:
            return json.loads(result.read())
    except HTTPError as exc:
        raise RuntimeError("GitHub HTTP %d: %s" % (exc.code, exc.read().decode("utf-8", "replace")[:250])) from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    token = os.getenv("GH_TOKEN")
    if not token:
        raise RuntimeError("GH_TOKEN required")
    if os.getenv("GITHUB_REPOSITORY", REPO) != REPO:
        raise RuntimeError("Wrong repository")
    found = set()
    for page in range(1, 10):
        data = request("GET", "repos/%s/labels?per_page=100&page=%d" % (REPO, page), token)
        found.update(x["name"] for x in data)
        if len(data) < 100:
            break
    missing = [(name, color, description) for name, (color, description) in LABELS.items() if name not in found]
    for name, color, description in missing:
        print(("CREATE " if args.apply else "PREVIEW ") + name)
        if args.apply:
            request("POST", "repos/%s/labels" % REPO, token,
                    {"name": name, "color": color, "description": description})
    print("Already present:", len(LABELS) - len(missing), "Missing:", len(missing), "Applied:", args.apply)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError) as exc:
        print("Label setup failed: " + str(exc), file=sys.stderr)
        sys.exit(2)

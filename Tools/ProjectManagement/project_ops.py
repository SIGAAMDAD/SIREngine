#!/usr/bin/env python3
"""SIREngine GitHub Projects V2 operator. Standard-library only.

All project mutations require --apply and an owner-provisioned project token.
No issue label, board field, or inferred relationship confers implementation
or merge authority. Project metadata never changes source branches or PR state.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import re
import sys
from urllib.error import HTTPError
from urllib.request import Request, urlopen

REPOSITORY = "SIGAAMDAD/SIREngine"
TRACKED = "marathon:tracked"
STAGES = (
    ("Proposed", "GRAY"), ("Approved", "BLUE"), ("Ready", "GREEN"),
    ("In progress", "YELLOW"), ("In review", "PURPLE"),
    ("Verification", "BLUE"), ("Awaiting merge approval", "ORANGE"),
    ("Blocked", "RED"), ("Done", "GREEN"), ("Closed without merge", "GRAY"),
)
SUBSYSTEMS = (
    "Core", "Renderer", "GRDG", "Shaders", "OpenGL", "DirectX", "Vulkan",
    "Meshing", "Terrain", "Physics", "Audio", "Input", "Scripting",
    "Console", "Editor", "World", "Networking", "QA", "Other",
)
FIELD_SPEC = {
    "Delivery Stage": ("SINGLE_SELECT", STAGES),
    "Priority": ("SINGLE_SELECT", tuple((x, "RED" if x == "P0" else "GRAY") for x in ("P0", "P1", "P2", "P3"))),
    "Subsystem": ("SINGLE_SELECT", tuple((x, "GRAY") for x in SUBSYSTEMS)),
    "Work Type": ("SINGLE_SELECT", tuple((x, "GRAY") for x in ("Roadmap", "Milestone", "Implementation", "Bug", "Verification", "Documentation", "PR"))),
    "Pipeline Era": ("SINGLE_SELECT", tuple((x, "GRAY") for x in ("Fixed function", "Programmable", "Modern", "Cross-era"))),
    "Plan Revision": ("TEXT", ()),
    "Worker ID": ("TEXT", ()),
    "Target Branch": ("TEXT", ()),
    "Integration Wave": ("TEXT", ()),
    "Coverage (%)": ("NUMBER", ()),
    "Target Date": ("DATE", ()),
}
VIEW_SPEC = (
    ("Master Backlog", "TABLE_LAYOUT", "label:marathon:tracked"),
    ("Intake", "TABLE_LAYOUT", "is:issue -label:marathon:ready"),
    ("Roadmaps", "ROADMAP_LAYOUT", "label:kind:roadmap"),
    ("Milestones", "ROADMAP_LAYOUT", "label:kind:milestone"),
    ("Ready for AI", "TABLE_LAYOUT", "is:issue is:open label:marathon:ready"),
    ("Active Development", "BOARD_LAYOUT", "is:issue is:open"),
    ("PR Review", "TABLE_LAYOUT", "is:pr is:open"),
    ("Verification", "TABLE_LAYOUT", "label:kind:verification"),
    ("Blocked", "TABLE_LAYOUT", "label:marathon:blocked"),
    ("Completed History", "TABLE_LAYOUT", "is:closed"),
)
PROJECT_QUERY = """
query($id:ID!,$after:String){
  node(id:$id) {
    ... on ProjectV2 {
      id title
      fields(first:100) {
        nodes {
          ... on ProjectV2FieldCommon { id name dataType }
          ... on ProjectV2SingleSelectField { options { id name } }
        }
      }
      views(first:100) { nodes { id name layout } }
      items(first:100,after:$after) {
        pageInfo { hasNextPage endCursor }
        nodes {
          id
          content {
            ... on Issue { id number repository { nameWithOwner } }
            ... on PullRequest { id number repository { nameWithOwner } }
          }
          fieldValues(first:30) {
            nodes {
              ... on ProjectV2ItemFieldSingleSelectValue {
                name field { ... on ProjectV2FieldCommon { name } }
              }
              ... on ProjectV2ItemFieldTextValue {
                text field { ... on ProjectV2FieldCommon { name } }
              }
              ... on ProjectV2ItemFieldNumberValue {
                number field { ... on ProjectV2FieldCommon { name } }
              }
            }
          }
        }
      }
    }
  }
}
"""


class ProjectError(RuntimeError):
    pass


class GitHub:
    def __init__(self, token: str):
        if not token:
            raise ProjectError("GitHub token is required")
        self.token = token

    def request(self, method: str, path: str, body: dict | None = None):
        data = json.dumps(body, separators=(",", ":")).encode() if body is not None else None
        headers = {
            "Accept": "application/vnd.github+json",
            "Authorization": "Bearer " + self.token,
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "SIREngine-Project-Operations",
        }
        req = Request("https://api.github.com/" + path.lstrip("/"), data=data, headers=headers, method=method)
        try:
            with urlopen(req, timeout=35) as response:
                return json.loads(response.read())
        except HTTPError as exc:
            message = exc.read().decode("utf-8", "replace")[:300]
            raise ProjectError("GitHub HTTP %d: %s" % (exc.code, message)) from None

    def graphql(self, query: str, **variables):
        response = self.request("POST", "graphql", {"query": query, "variables": variables})
        if response.get("errors"):
            raise ProjectError("GitHub GraphQL: " + json.dumps(response["errors"])[:1200])
        return response["data"]

    def issues(self, repo: str):
        page = 1
        while True:
            rows = self.request("GET", "repos/%s/issues?state=all&per_page=100&page=%d" % (repo, page))
            if not isinstance(rows, list):
                raise ProjectError("Invalid issue listing")
            yield from rows
            if len(rows) < 100:
                return
            page += 1


def resolve_project(api: GitHub, owner: str, title: str, number: int | None):
    query = """query($owner:String!){user(login:$owner){
      projectsV2(first:100){nodes{id number title}}
    }}"""
    data = api.graphql(query, owner=owner)
    rows = (data.get("user") or {}).get("projectsV2", {}).get("nodes", [])
    matches = [p for p in rows if (p["number"] == number if number else p["title"] == title)]
    if len(matches) != 1:
        raise ProjectError("Project not uniquely found; set SIR_PROJECT_NUMBER (owner %s)" % owner)
    if number and matches[0]["title"] != title:
        raise ProjectError("Project number points to a different title")
    return matches[0]


def project_snapshot(api: GitHub, project_id: str):
    after = None
    result = {"fields": [], "views": [], "items": []}
    while True:
        data = api.graphql(PROJECT_QUERY, id=project_id, after=after)
        node = data["node"]
        if not node:
            raise ProjectError("Project inaccessible")
        result["fields"] = node["fields"]["nodes"]
        result["views"] = node["views"]["nodes"]
        result["items"].extend(node["items"]["nodes"])
        info = node["items"]["pageInfo"]
        if not info["hasNextPage"]:
            break
        after = info["endCursor"]
        if not after:
            raise ProjectError("Project cursor incomplete")
    return result


def ensure_schema(api: GitHub, project_id: str, snapshot: dict, apply: bool):
    fields = {f["name"]: f for f in snapshot["fields"]}
    views = {v["name"]: v for v in snapshot["views"]}
    # Validate the entire schema before the first mutation.
    for name, (kind, _) in FIELD_SPEC.items():
        if name in fields and fields[name]["dataType"] != kind:
            raise ProjectError("Existing field has wrong type: " + name)
    actions = []
    for name, (kind, options) in FIELD_SPEC.items():
        if name in fields:
            continue
        actions.append("field " + name)
        if apply:
            payload = {"projectId": project_id, "name": name, "dataType": kind}
            if kind == "SINGLE_SELECT":
                payload["singleSelectOptions"] = [
                    {"name": value, "description": "", "color": color} for value, color in options
                ]
            api.graphql(
                "mutation($input:CreateProjectV2FieldInput!){createProjectV2Field(input:$input){projectV2Field{... on ProjectV2FieldCommon{id name}}}}",
                input=payload,
            )
    for name, layout, filter_text in VIEW_SPEC:
        if name in views:
            continue  # Existing team views are never overwritten.
        actions.append("view " + name)
        if apply:
            data = api.graphql(
                "mutation($input:CreateProjectV2ViewInput!){createProjectV2View(input:$input){projectV2View{id}}}",
                input={"projectId": project_id, "name": name, "layout": layout},
            )
            view_id = data["createProjectV2View"]["projectV2View"]["id"]
            api.graphql(
                "mutation($input:UpdateProjectV2ViewInput!){updateProjectV2View(input:$input){projectV2View{id}}}",
                input={"viewId": view_id, "filter": filter_text},
            )
    return actions


def labels_of(issue: dict) -> set[str]:
    return {label["name"] for label in issue.get("labels", []) if isinstance(label, dict) and "name" in label}


def delivery_stage(issue: dict) -> str:
    labels = labels_of(issue)
    if issue["state"] == "closed":
        if "pull_request" in issue:
            return "Done" if issue.get("pull_request", {}).get("merged_at") else "Closed without merge"
        return "Closed without merge" if issue.get("state_reason") == "not_planned" else "Done"
    if "pull_request" in issue:
        return "In review"  # Draft vs ready is resolved in the optional PR metadata fetch.
    if "marathon:blocked" in labels:
        return "Blocked"
    if "marathon:ready" in labels:
        return "Ready"
    if "marathon:approved" in labels:
        return "Approved"
    if "marathon:active" in labels:
        return "In progress"
    return "Proposed"


PREFIXES = {
    "GL": "OpenGL", "D": "DirectX", "D3D": "DirectX", "VK": "Vulkan",
    "SHD": "Shaders", "VG": "Meshing", "MESH": "Meshing",
    "GRDG": "GRDG", "CONSOLE": "Console", "INPUT": "Input",
    "SIRLANG": "Scripting", "SCR": "Scripting", "CORE": "Core",
    "TERRAIN": "Terrain", "PHY": "Physics", "AUDIO": "Audio",
    "EDITOR": "Editor", "NET": "Networking", "WORLD": "World",
}


def subsystem(issue: dict) -> str | None:
    for label in labels_of(issue):
        if label.startswith("subsystem:"):
            name = label.split(":", 1)[1].lower()
            return next((s for s in SUBSYSTEMS if s.lower() == name), None)
    match = re.match(r"^\[([A-Za-z0-9_-]+)", issue.get("title", ""))
    if not match:
        return None
    key = re.split(r"[-_0-9]", match.group(1), 1)[0].upper()
    return PREFIXES.get(key)


def work_type(issue: dict) -> str:
    if "pull_request" in issue:
        return "PR"
    for value in ("Roadmap", "Milestone", "Implementation", "Bug", "Verification", "Documentation"):
        if "kind:" + value.lower() in labels_of(issue):
            return value
    title = issue.get("title", "").lower()
    if title.startswith("[roadmap]"):
        return "Roadmap"
    if "milestone" in title or re.match(r"^\[console p\d+\]", title):
        return "Milestone"
    if title.startswith("[bug]"):
        return "Bug"
    if title.startswith("[docs]"):
        return "Documentation"
    return "Implementation"


def explicit_parent(issue: dict) -> int | None:
    body = issue.get("body") or ""
    match = re.search(r"(?im)\b(?:canonical parent|parent issue)\b[^#\n]{0,16}#(\d+)", body)
    return int(match.group(1)) if match else None


def field_values(issue: dict, pr: dict | None = None) -> dict[str, str]:
    labels = labels_of(issue)
    values = {"Delivery Stage": delivery_stage(issue), "Work Type": work_type(issue)}
    subsys = subsystem(issue)
    if subsys:
        values["Subsystem"] = subsys
    for value in ("P0", "P1", "P2", "P3"):
        if "priority:" + value.lower() in labels:
            values["Priority"] = value
            break
    for era, text in (
        ("fixed", "Fixed function"), ("programmable", "Programmable"),
        ("modern", "Modern"), ("cross-era", "Cross-era"),
    ):
        if "era:" + era in labels:
            values["Pipeline Era"] = text
            break
    for label in labels:
        for prefix, field in (("wave:", "Integration Wave"), ("worker:", "Worker ID")):
            if label.startswith(prefix):
                values[field] = label.split(":", 1)[1][:80]
    body = issue.get("body") or ""
    match = re.search(r"(?im)\bplan revision\b[^\n:]{0,4}:\s*[\x60*]*([a-z0-9._-]{1,80})", body)
    if match:
        values["Plan Revision"] = match.group(1)
    if pr and pr.get("base"):
        values["Target Branch"] = pr["base"]["ref"]
        if pr.get("draft") and issue["state"] == "open":
            values["Delivery Stage"] = "In progress"
    return values


def existing_values(item: dict) -> dict[str, str | float]:
    result = {}
    for field in item.get("fieldValues", {}).get("nodes", []):
        name = (field.get("field") or {}).get("name")
        if name:
            for value_key in ("name", "text", "number"):
                if field.get(value_key) is not None:
                    result[name] = field[value_key]
                    break
    return result


def update_item_fields(api: GitHub, project_id: str, item: dict, issue: dict, fields: dict, pr: dict | None, apply: bool):
    values = field_values(issue, pr)
    old = existing_values(item)
    changes = []
    for name, value in values.items():
        if name not in fields:
            continue
        if name == "Delivery Stage" and "marathon:manual-stage" in labels_of(issue):
            continue
        if old.get(name) == value:
            continue
        if name not in ("Delivery Stage", "Priority", "Plan Revision", "Target Branch") and old.get(name):
            continue  # Preserve human classification and worker assignments.
        field = fields[name]
        data_type = field["dataType"]
        if data_type == "SINGLE_SELECT":
            option = next((x for x in field.get("options", []) if x["name"] == value), None)
            if option is None:
                raise ProjectError("Missing project option %s / %s" % (name, value))
            payload = {"singleSelectOptionId": option["id"]}
        elif data_type == "TEXT":
            payload = {"text": str(value)}
        else:
            continue
        changes.append("%s=%s" % (name, value))
        if apply:
            api.graphql(
                "mutation($input:UpdateProjectV2ItemFieldValueInput!){updateProjectV2ItemFieldValue(input:$input){projectV2Item{id}}}",
                input={"projectId": project_id, "itemId": item["id"], "fieldId": field["id"], "value": payload},
            )
    return changes


def sync(api: GitHub, repo: str, project_id: str, snapshot: dict, apply: bool, number: int | None, limit: int):
    project_items = {
        row["content"]["id"]: row for row in snapshot["items"] if row.get("content") and row["content"].get("id")
    }
    fields = {field["name"]: field for field in snapshot["fields"]}
    source = [api.request("GET", "repos/%s/issues/%d" % (repo, number))] if number else list(api.issues(repo))
    changed = 0
    skipped = 0
    processed = 0
    for issue in source:
        if TRACKED not in labels_of(issue) and not number:
            skipped += 1
            continue
        if limit and processed >= limit:
            break
        processed += 1
        content_id = issue.get("node_id")
        if not content_id:
            raise ProjectError("Missing node id for item #%d" % issue["number"])
        item = project_items.get(content_id)
        if not item:
            print("ADD #%d %s" % (issue["number"], issue["title"][:70]))
            changed += 1
            if apply:
                data = api.graphql(
                    "mutation($input:AddProjectV2ItemByIdInput!){addProjectV2ItemById(input:$input){item{id}}}",
                    input={"projectId": project_id, "contentId": content_id},
                )
                item = {"id": data["addProjectV2ItemById"]["item"]["id"], "fieldValues": {"nodes": []}}
                project_items[content_id] = item
        pr = None
        if "pull_request" in issue and apply:
            pr = api.request("GET", "repos/%s/pulls/%d" % (repo, issue["number"]))
            issue["pull_request"]["merged_at"] = pr.get("merged_at")
        if item:
            edits = update_item_fields(api, project_id, item, issue, fields, pr, apply)
            if edits:
                changed += len(edits)
                print("SET #%d %s" % (issue["number"], ", ".join(edits)))
    return {"processed": processed, "skipped_untracked": skipped, "mutations_planned": changed, "applied": apply}


def link_hierarchy(api: GitHub, repo: str, apply: bool, limit: int):
    issues = {x["number"]: x for x in api.issues(repo) if "pull_request" not in x}
    edges = []
    for child in issues.values():
        parent = explicit_parent(child)
        if parent and parent != child["number"] and parent in issues:
            edges.append((parent, child["number"]))
    edges.sort()
    linked = 0
    for parent, child in edges[:limit or None]:
        payload = {"parent": parent, "child": child}
        if apply:
            data = api.graphql(
                "query($id:ID!){node(id:$id){... on Issue{parent{id}}}}",
                id=issues[child]["node_id"],
            )
            existing = ((data.get("node") or {}).get("parent") or {}).get("id")
            if existing:
                if existing == issues[parent]["node_id"]:
                    continue
                print("SKIP #%d: already has another parent" % child)
                continue
            api.graphql(
                "mutation($input:AddSubIssueInput!){addSubIssue(input:$input){issue{id} subIssue{id}}}",
                input={"issueId": issues[parent]["node_id"], "subIssueId": issues[child]["node_id"], "replaceParent": False},
            )
        print("PARENT #%d -> #%d" % (parent, child))
        linked += 1
    return {"explicit_edges": len(edges), "processed": linked, "applied": apply}


def report(api: GitHub, repo: str, directory: Path):
    issues = list(api.issues(repo))
    states = Counter()
    types = Counter()
    stages = Counter()
    subsystems = Counter()
    now = datetime.now(timezone.utc)
    created_7 = merged_7 = closed_7 = 0
    for item in issues:
        is_pr = "pull_request" in item
        states[("PR" if is_pr else "Issue") + "/" + item["state"]] += 1
        types[work_type(item)] += 1
        stages[delivery_stage(item)] += 1
        subsystems[subsystem(item) or "Unclassified"] += 1
        created = datetime.fromisoformat(item["created_at"].replace("Z", "+00:00"))
        if created >= now - timedelta(days=7):
            created_7 += 1
        if item.get("closed_at"):
            closed = datetime.fromisoformat(item["closed_at"].replace("Z", "+00:00"))
            if closed >= now - timedelta(days=7):
                closed_7 += 1
                if is_pr and item["pull_request"].get("merged_at"):
                    merged_7 += 1
    stale = sorted(
        (x for x in issues if x["state"] == "open"
         and datetime.fromisoformat(x["updated_at"].replace("Z", "+00:00")) < now - timedelta(days=14)),
        key=lambda x: x["updated_at"],
    )
    blocked = sorted(
        (x for x in issues if x["state"] == "open" and "marathon:blocked" in labels_of(x)),
        key=lambda x: x["number"],
    )
    metrics = {
        "observed_at": now.isoformat(), "repository": repo, "total": len(issues),
        "states": dict(sorted(states.items())), "work_types": dict(sorted(types.items())),
        "stages": dict(sorted(stages.items())), "subsystems": dict(sorted(subsystems.items())),
        "last_7_days": {"created": created_7, "closed": closed_7, "merged_prs": merged_7},
        "attention": {
            "blocked_open": len(blocked), "stale_open_14d": len(stale),
            "blocked_issue_numbers": [x["number"] for x in blocked[:50]],
            "stale_issue_numbers": [x["number"] for x in stale[:50]],
        },
        "limitations": "Classification metadata, not measured engine implementation or QA coverage.",
    }
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    lines = ["# SIREngine GitHub workload report", "",
             "Snapshot: " + metrics["observed_at"], "",
             "This reports **GitHub item activity**, not engine completion or test coverage.", "",
             "| Item state | Count |", "|---|---:|"]
    lines.extend("| %s | %d |" % (name, count) for name, count in sorted(states.items()))
    lines.extend(["", "## Last seven days", "",
                  "- Created issues/PRs: %d" % created_7,
                  "- Closed issues/PRs: %d" % closed_7,
                  "- Merged PRs with observable merge timestamp: %d" % merged_7,
                  "", "## Subsystem breakdown", "",
                  "| Subsystem | Items |", "|---|---:|"])
    lines.extend("| %s | %d |" % (name, count) for name, count in sorted(subsystems.items()))
    lines.extend(["", "## Needs attention", "",
                  "- Open issues/PRs flagged blocked: %d" % len(blocked),
                  "- Open issues/PRs with no update in 14 days: %d" % len(stale), ""])
    for heading, rows in (("Blocked items", blocked[:20]), ("Stale items", stale[:20])):
        lines.extend(["### " + heading, ""])
        if not rows:
            lines.append("None in this snapshot.")
        for row in rows:
            lines.append("- [#%d](https://github.com/%s/issues/%d) — %s" %
                         (row["number"], repo, row["number"], "PR" if "pull_request" in row else "issue"))
        lines.append("")
    text = "\n".join(lines) + "\n"
    (directory / "summary.md").write_text(text, encoding="utf-8")
    step_summary = os.getenv("GITHUB_STEP_SUMMARY")
    if step_summary:
        with open(step_summary, "a", encoding="utf-8") as output:
            output.write(text)
    print(text)
    return metrics


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("provision", "sync", "backfill", "hierarchy", "report"))
    parser.add_argument("--repo", default=os.getenv("GITHUB_REPOSITORY", REPOSITORY))
    parser.add_argument("--owner", default=os.getenv("SIR_PROJECT_OWNER", "SIGAAMDAD"))
    parser.add_argument("--project-title", default=os.getenv("SIR_PROJECT_TITLE", "SIREngine"))
    parser.add_argument("--project-number", type=int, default=int(os.getenv("SIR_PROJECT_NUMBER") or "0"))
    parser.add_argument("--number", type=int, help="One issue or PR number for event-driven sync")
    parser.add_argument("--limit", type=int, default=0, help="Cap items in this invocation; zero means all")
    parser.add_argument("--apply", action="store_true", help="Actually mutate the existing project")
    parser.add_argument("--report-dir", type=Path, default=Path("project-reports"))
    args = parser.parse_args(argv)
    if args.repo != REPOSITORY:
        raise ProjectError("Project operator is locked to " + REPOSITORY)
    if args.number is not None and args.number < 1:
        parser.error("number must be positive")
    if args.limit < 0:
        parser.error("limit cannot be negative")
    token = (os.getenv("SIR_PROJECT_TOKEN") if args.operation != "report" else None) or os.getenv("GH_TOKEN")
    api = GitHub(token)
    if args.operation == "report":
        report(api, args.repo, args.report_dir)
        return 0
    if args.apply and not os.getenv("SIR_PROJECT_TOKEN"):
        raise ProjectError("Project mutations require the explicitly provisioned SIR_PROJECT_TOKEN")
    project = resolve_project(api, args.owner, args.project_title, args.project_number or None)
    project_id = project["id"]
    if args.operation == "hierarchy":
        print(json.dumps(link_hierarchy(api, args.repo, args.apply, args.limit), indent=2))
        return 0
    snapshot = project_snapshot(api, project_id)
    if args.operation == "provision":
        print(json.dumps({"project": project, "changes": ensure_schema(api, project_id, snapshot, args.apply),
                          "applied": args.apply}, indent=2))
    else:
        print(json.dumps(sync(api, args.repo, project_id, snapshot, args.apply,
                              args.number if args.operation == "sync" else None, args.limit), indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ProjectError as exc:
        print("SIREngine project operation failed: " + str(exc), file=sys.stderr)
        sys.exit(2)

"""Offline acceptance tests for the SIREngine GitHub Projects operator."""
from __future__ import annotations

import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import project_ops as ops


def item(number=101, title="[GL33-01] Build driver", labels=(), state="open", body=""):
    return {
        "number": number,
        "node_id": "ISSUE_NODE_" + str(number),
        "title": title,
        "labels": [{"name": name} for name in labels],
        "state": state,
        "body": body,
        "created_at": "2026-11-04T19:00:00Z",
    }


class Fake:
    def __init__(self, issues=None):
        self.rows = issues or []
        self.calls = []

    def issues(self, repo):
        self.calls.append(("issues", repo))
        return iter(self.rows)

    def request(self, method, path, body=None):
        self.calls.append((method, path, body))
        if "/issues/" in path:
            return next(row for row in self.rows if str(row["number"]) == path.rsplit("/", 1)[-1])
        return {"base": {"ref": "develop"}, "draft": False, "merged_at": None}

    def graphql(self, query, **variables):
        self.calls.append(("graphql", query, variables))
        if "addProjectV2ItemById" in query:
            return {"addProjectV2ItemById": {"item": {"id": "NEW_ITEM"}}}
        if "createProjectV2Field" in query:
            return {"createProjectV2Field": {"projectV2Field": {"id": "NEW_FIELD"}}}
        if "createProjectV2View" in query:
            return {"createProjectV2View": {"projectV2View": {"id": "NEW_VIEW"}}}
        if "updateProjectV2View" in query:
            return {"updateProjectV2View": {"projectV2View": {"id": "NEW_VIEW"}}}
        if "updateProjectV2ItemFieldValue" in query:
            return {"updateProjectV2ItemFieldValue": {"projectV2Item": {"id": "NEW_ITEM"}}}
        if "addSubIssue" in query:
            return {"addSubIssue": {"issue": {"id": "p"}, "subIssue": {"id": "c"}}}
        if "... on Issue{parent{id}}" in query:
            return {"node": {"parent": None}}
        raise AssertionError("Unrecognized fake GraphQL operation")


class ClassificationTests(unittest.TestCase):
    def test_new_issue_is_proposed(self):
        self.assertEqual(ops.delivery_stage(item()), "Proposed")

    def test_ready_not_approval(self):
        self.assertEqual(ops.delivery_stage(item(labels=["marathon:ready"])), "Ready")

    def test_owner_approval_label_is_display_only(self):
        self.assertEqual(ops.delivery_stage(item(labels=["marathon:approved"])), "Approved")

    def test_blocked_takes_precedence(self):
        self.assertEqual(ops.delivery_stage(item(labels=["marathon:blocked", "marathon:ready"])), "Blocked")

    def test_closed_issue(self):
        self.assertEqual(ops.delivery_stage(item(state="closed")), "Done")

    def test_not_planned_issue(self):
        x = item(state="closed")
        x["state_reason"] = "not_planned"
        self.assertEqual(ops.delivery_stage(x), "Closed without merge")

    def test_merged_pull_request(self):
        x = item(state="closed")
        x["pull_request"] = {"merged_at": "2026-11-04T19:00:00Z"}
        self.assertEqual(ops.delivery_stage(x), "Done")

    def test_unmerged_pull_request(self):
        x = item(state="closed")
        x["pull_request"] = {"merged_at": None}
        self.assertEqual(ops.delivery_stage(x), "Closed without merge")

    def test_draft_pr(self):
        x = item()
        x["pull_request"] = {"merged_at": None}
        self.assertEqual(ops.field_values(x, {"draft": True, "base": {"ref": "develop"}}),
                         {"Delivery Stage": "In progress", "Work Type": "PR", "Target Branch": "develop", "Subsystem": "OpenGL"})

    def test_subsystem_prefix(self):
        self.assertEqual(ops.subsystem(item(title="[D11-14] Native swapchain")), "DirectX")
        self.assertEqual(ops.subsystem(item(title="[VG-R3-04] Geometry backing")), "Meshing")
        self.assertEqual(ops.subsystem(item(title="[SHD-00] Shader ABI")), "Shaders")

    def test_explicit_labels_override_guess(self):
        self.assertEqual(ops.subsystem(item(labels=["subsystem:audio"])), "Audio")

    def test_type_detection(self):
        self.assertEqual(ops.work_type(item(title="[Roadmap] SHD")), "Roadmap")
        self.assertEqual(ops.work_type(item(title="[Console P4] CVar")), "Milestone")
        self.assertEqual(ops.work_type(item(title="[Bug] Fault")), "Bug")

    def test_dependency_requires_explicit_parent(self):
        self.assertEqual(ops.explicit_parent(item(body="**Canonical parent:** #36 (VG)")), 36)
        self.assertEqual(ops.explicit_parent(item(body="Parents #3, #4 for general context")), None)

    def test_plan_revision(self):
        values = ops.field_values(item(body="Plan revision: gl46-r2", labels=["priority:p0", "era:modern"]))
        self.assertEqual(values["Plan Revision"], "gl46-r2")
        self.assertEqual(values["Priority"], "P0")
        self.assertEqual(values["Pipeline Era"], "Modern")


class DataSafetyTests(unittest.TestCase):
    def test_schema_dryrun_never_mutates(self):
        fake = Fake()
        changes = ops.ensure_schema(fake, "project", {"fields": [], "views": []}, False)
        self.assertGreater(len(changes), 10)
        self.assertEqual(fake.calls, [])

    def test_schema_type_collision_fails(self):
        fake = Fake()
        with self.assertRaises(ops.ProjectError):
            ops.ensure_schema(fake, "p", {"fields": [{"name": "Priority", "dataType": "TEXT"}], "views": []}, True)
        self.assertEqual(fake.calls, [])

    def test_backfill_skips_untracked_and_pr_preserves_identity(self):
        x = item(number=99, labels=["marathon:tracked"])
        y = item(number=102)
        fake = Fake([x, y])
        result = ops.sync(fake, ops.REPOSITORY, "P", {"items": [], "fields": []}, False, None, 0)
        self.assertEqual(result["processed"], 1)
        self.assertEqual(result["skipped_untracked"], 1)
        self.assertEqual(result["mutations_planned"], 1)
        self.assertFalse(any(call[0] == "graphql" for call in fake.calls))

    def test_event_sync_includes_new_unlabeled_issue(self):
        x = item(number=99)
        fake = Fake([x])
        result = ops.sync(fake, ops.REPOSITORY, "P", {"items": [], "fields": []}, False, 99, 0)
        self.assertEqual(result["processed"], 1)

    def test_existing_status_no_rewrite(self):
        x = item(labels=["marathon:tracked"])
        existing = {"id": "i", "content": {"id": x["node_id"]},
                    "fieldValues": {"nodes": [{"name": "Proposed", "field": {"name": "Delivery Stage"}}]}}
        option = {"id": "o", "name": "Proposed"}
        f = {"name": "Delivery Stage", "id": "F", "dataType": "SINGLE_SELECT", "options": [option]}
        fake = Fake([x])
        result = ops.sync(fake, ops.REPOSITORY, "P", {"items": [existing], "fields": [f]}, True, None, 0)
        self.assertEqual(result["mutations_planned"], 0)

    def test_manual_stage_not_overridden(self):
        x = item(labels=["marathon:tracked", "marathon:ready", "marathon:manual-stage"])
        existing = {"id": "i", "content": {"id": x["node_id"]},
                    "fieldValues": {"nodes": [{"name": "Verification", "field": {"name": "Delivery Stage"}}]}}
        f = {"name": "Delivery Stage", "id": "F", "dataType": "SINGLE_SELECT",
             "options": [{"id": "r", "name": "Ready"}]}
        fake = Fake([x])
        result = ops.sync(fake, ops.REPOSITORY, "P", {"items": [existing], "fields": [f]}, True, None, 0)
        self.assertEqual(result["mutations_planned"], 0)

    def test_hierarchy_read_only_does_not_emit_mutation(self):
        x = item(number=5, body="Canonical parent: #4")
        y = item(number=4)
        fake = Fake([x, y])
        result = ops.link_hierarchy(fake, ops.REPOSITORY, False, 0)
        self.assertEqual(result["explicit_edges"], 1)
        self.assertEqual(result["processed"], 1)
        self.assertFalse(any(call[0] == "graphql" for call in fake.calls))

    def test_ensure_project_resolves_exact_title(self):
        class ProjectFake(Fake):
            def graphql(self, query, **variables):
                return {"user": {"projectsV2": {"nodes": [
                    {"id": "a", "title": "SIREngine", "number": 12},
                    {"id": "b", "title": "Other", "number": 13},
                ]}}}
        self.assertEqual(ops.resolve_project(ProjectFake(), "SIGAAMDAD", "SIREngine", None)["id"], "a")

    def test_duplicate_project_name_rejected(self):
        class ProjectFake(Fake):
            def graphql(self, query, **variables):
                return {"user": {"projectsV2": {"nodes": [
                    {"id": "a", "title": "SIREngine", "number": 12},
                    {"id": "b", "title": "SIREngine", "number": 13},
                ]}}}
        with self.assertRaises(ops.ProjectError):
            ops.resolve_project(ProjectFake(), "SIGAAMDAD", "SIREngine", None)


if __name__ == "__main__":
    unittest.main()

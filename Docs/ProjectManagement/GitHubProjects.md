# SIREngine GitHub Project — AI-first, team-maintained operations

This repository uses **GitHub Projects V2** as its human-visible project board. GitHub Issues are the implementation backlog, PRs carry actual source work, and Actions provide reproducible evidence. No external coordinator or SQLite is part of this setup.

## Authority

The repository owner is the final authority for architecture, plan revisions, implementation scope, and integration. AI-created issues are proposals until owner approval of their exact plan revision and named work-package IDs. An approval to implement does not authorize merging. Human maintainers may later use the same review, labeling, CI and PR process.

## Project configuration (one-time GitHub Projects UI)

1. Open the existing **SIREngine** GitHub Project, then **Workflows > Auto-add to project > Edit**.
2. Select repository `SIGAAMDAD/SIREngine` and filter **`is:issue,pr label:"marathon:tracked"`**.
3. Save and turn on the workflow. This single filter covers **issues and PRs**, not just issues.
4. GitHub Auto-add does not retroactively enroll existing matching items. In the project, use **Add item > Add from repository**, select the SIREngine repository, and bulk-add existing matching issues/PRs, including historical closed work if desired. Search `label:marathon:tracked`. Confirm the board's item count directly.
5. Configure a shared **Status** field with `Proposed`, `Approved`, `Ready`, `In progress`, `In review`, `Verification`, `Awaiting merge approval`, `Blocked`, `Done`. This is a suggested board workflow, not a new authorization mechanism.
6. Suggested optional fields: `Priority` (P0-P3), `Subsystem`, `Work type` (roadmap/milestone/task/PR), `Plan revision`, `Owning team`, `Target branch`. Create only fields used by maintainers.
7. Set up saved views: **Intake** (new/proposed), **Roadmaps** (milestones), **Implementation** (ready/in-progress), **PR review** (PRs/in-review), **Verification**, **Blocked**, and **Archive** (closed/merged). Preserve existing IDs and history.
8. Enable default GitHub **Item closed** / **Pull request merged** status automation if available; verify results on representative issues/PRs.

The GitHub connector can label and update issues and PRs, but does not expose native Projects V2 custom-field/board mutation. Do not treat the repository label alone as proof of membership.

## Intake and approval labels

| Label | Meaning | Authorization |
|---|---|---|
| `marathon:tracked` | Board intake for issue or PR | Administrative only |
| `marathon:approved` | Owner approved the referenced plan revision / issue scope | Owner record required |
| `marathon:ready` | Director checked prerequisites and source ownership | Never sufficient by itself |
| `marathon:merge-approved` | Owner approved an exact PR head and destination | Separate owner record required |

The `project-intake.yml` workflow on the default branch auto-labels newly opened/reopened issues and PRs. Its `pull_request_target` event performs only a trusted GitHub label mutation; it never checks out or runs PR code.

## Lifecycle

1. A planning chat creates or updates a roadmap/milestone issue with a revision, explicit work-package keys, dependency graph, source ownership, tests and implementation boundaries.
2. Owner approves exact plan revision and work packages; log evidence in GitHub. The director may create only those authorized child tasks.
3. Director marks eligible tasks ready; worker claims one issue, implements on its branch, records exact SHA and verification evidence, and opens/updates PR.
4. Independent verifier reports `PASS`, `FAIL`, `NOT_RUN`, `BLOCKED` or `TIMED_OUT` with exact-SHA context.
5. After owner approves exact PR revision/base and required checks pass, an authorized integrator merges, verifies remote merged SHA, closes the task, and re-evaluates the parent milestone.
6. New team contributors follow the same issue/PR interface; historical issue URLs, authorship and commit history remain unchanged.

## Migration inventory

On 2026-11-04 the repository had **239 issues** (232 open, 7 closed) and **40 pull requests** (12 open, 28 closed). All **279 records** were labeled `marathon:tracked` through the GitHub connector. This is confirmed repository labeling, **not independently verified Project board membership**. GitHub Projects' initial bulk-add and Auto-add workflow must still be configured in the Project UI.

## Team handover checks

- New issue and new PR both acquire `marathon:tracked` from the workflow after the configuration PR is merged to default branch.
- Both appear in the SIREngine Project once Auto-add is enabled.
- A proposal remains ineligible to Marathon without owner-approved scope and revision.
- A new PR head SHA invalidates an older PR-specific merge approval.
- Closed issues and merged PRs remain discoverable in Archive.
- No issue is closed simply because a PR exists; source integration and verification evidence are required.

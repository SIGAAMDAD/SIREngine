# SIREngine GitHub Project — manual-first operations

**Project model:** AI writes the initial plans and implementation under human authority; the same GitHub Project and issue/PR workflow can later be maintained by a human team. The project board is the visible work queue. There is no SQLite database, background scheduler, custom Projects GraphQL operator, project token, or mandatory repository-side project-management script.

**Human authority:** The owner approves architecture, scope, plan revisions, and integration. A board item or label never grants those permissions by itself. A worker launched with an explicit owner instruction may implement exactly one suitable existing issue within the authorized selection set; it must record the issue number, revision and authorization when claiming. Changed scope needs a new owner decision. PR merging requires separate approval of its exact head SHA and destination.

## One-time GitHub Project UI setup

The connected GitHub app exposes Issues, PRs and repository files, but no Projects V2 fields, views, or board-membership mutation. **These UI actions remain unverified until a human project administrator completes them:**

1. Open the existing **SIREngine** GitHub Project and select **Workflows > Auto-add to project > Edit**. Select repository `SIGAAMDAD/SIREngine`; use `is:issue,pr label:"marathon:tracked"`; choose **Save and turn on workflow**. This supports both issues and PRs.
2. Bulk-add **existing** SIREngine records using **Add item > Add from repository**, filtering `label:marathon:tracked`. Auto-add only handles newly created or updated matching items, not historic backfill. Verify actual board membership, including closed issues/merged PRs.
3. Keep GitHub's native **Status** field simple: **Todo**, **In progress**, **Done**. Use issue/PR labels for more detailed state. **Item added to project** can set Todo. Enable **Issue closed** and **PR merged** → Done. **Do not enable a workflow that closes an issue simply because its board status becomes Done**.
4. Create saved views described below, using each exact filter. Views reorganize the same records; they do not create or duplicate issues.
5. Add optional `Priority` and `Subsystem` single-select fields only if the team needs them. Labels are the authoritative filterable metadata available to manually launched chats through the current connector.
6. Populate the Project README/About with a link to this file, issue #281, the linked GitHub repository, and the Manual Worker Quickstart. No new project or external dashboard is needed.

## Recommended saved views

| View | Layout | Filter | Manual purpose |
|---|---|---|---|
| All work | Table | `repo:SIGAAMDAD/SIREngine label:marathon:tracked` | Full inventory |
| Pick a task | Table | `is:issue is:open -label:kind:roadmap -label:marathon:active -label:marathon:blocked` | Candidate pool; worker verifies owner authority, dependency and paths before selection |
| In progress | Board | `is:issue is:open label:marathon:active` | Current claimed tasks |
| Roadmaps | Table | `is:issue label:kind:roadmap` | Subsystem plans, parent issues |
| Milestones | Roadmap | `is:issue label:kind:milestone` | Parent progress and native sub-issues |
| PR review | Table | `is:pr is:open` | Drafts, reviews and exact-SHA QA |
| Blocked | Table | `is:open label:marathon:blocked` | Unmet dependencies and unresolved ownership |
| Verification | Table | `is:open label:kind:verification` | Explicit source/QA gates |
| Completed history | Table | `is:closed` | Verified completed and historical work |

For Pick a task, sort by Priority if maintained, or choose uniformly at random from the **verified eligible** candidates. A saved view is a shortlist, not proof a task is approved or conflict-free. Use GitHub's native sub-issues on parent milestones where available and supported, rather than inventing duplicate tracking documents.

## Minimal labels

- `marathon:tracked`: administrative issue/PR intake only. The `.github/workflows/project-intake.yml` default-branch workflow labels newly opened and reopened issues/PRs.
- `kind:roadmap`, `kind:milestone`, `kind:implementation`, `kind:verification`, `kind:bug`, `kind:documentation`: work type.
- `subsystem:*`, `priority:p0` through `priority:p3`: optional classification.
- `marathon:approved`: a pointer to separately verified owner approval for the **exact** plan revision; never apply it without the decision.
- `marathon:ready`: requirements checked; a worker still verifies approval and dependencies.
- `marathon:active`: manually claimed/in-progress; not an atomic exclusive lock.
- `marathon:blocked`: cannot proceed with a documented blocker.
- `marathon:merge-approved`: a pointer to a separate owner decision for the exact PR head/base; the label alone never suffices.

Do not treat a sub-issue as complete until its merged source and acceptance evidence are confirmed. Close a parent milestone only after all required children and gates pass.

## Manual worker lifecycle

1. The owner starts a ChatGPT worker using the [Manual Worker Quickstart](ManualWorkerQuickstart.md) and grants permission to execute a defined existing issue or a random choice from an explicitly authorized set.
2. Worker reads **current** open issue/PR data (paginate the complete backlog), excludes claimed/blocked/unauthorized work, checks the parent plan and dependencies, chooses a bounded work package, and checks path overlap.
3. If multiple workers may run, use the existing `ops/sirengine-control` branch: create a fixed `.sir-marathon/claims/issue-N.json` file, verify ownership/token and check again before publication. This uses GitHub API file operations, **not a local script**. One manual worker at a time can instead use a clearly documented issue claim, but labels/assignees are not atomic.
4. Mark `marathon:active`, post a comment with worker ID, branch, source baseline, exclusive paths, owner authorization and next acceptance gate. Recheck claim before publishing source changes.
5. Implement on the existing dedicated branch where appropriate, run required checks, commit, publish a PR and cite exact head SHA and evidence. Leave truthful NOT_RUN/FAIL/BLOCKED gates.
6. Request independent verification where required. Do not merge without owner approval. The owner or integrator merges verified, authorized PRs, then updates and closes verified child issues and parent milestones.
7. Re-read the queue and choose another **independent, authorized** task if the launch instruction permits more work. End on no eligible work, collision/blocker with no safe alternative, or execution limit; post a durable checkpoint.

**Random choice does not bypass dependency or ownership checks.** Broad roadmap parents are coordination containers; their implementation children are the normal execution units.

## Review and handover

- Issue forms: `.github/ISSUE_TEMPLATE/` for roadmap/milestone proposals, implementation packages, QA and bugs.
- PR checks: `.github/PULL_REQUEST_TEMPLATE.md`.
- Team review ownership: `.github/CODEOWNERS`.
- Contributor entry: `CONTRIBUTING.md`.
- Claim and checkpoint policy: `ops/sirengine-control` (already initialized).
- Project setup issue: https://github.com/SIGAAMDAD/SIREngine/issues/281.
- Initial configuration PR: https://github.com/SIGAAMDAD/SIREngine/pull/280.

**Limitations:** GitHub Projects V2's actual views, fields, status automations and existing record membership cannot be changed through the present connected GitHub app. This file defines the specific UI configuration, not proof of deployment. The repository-side template/workflow changes activate only after the owner authorizes and performs the PR merge to default branch.

## Tracking location

GitHub issues are the canonical milestone/task/status records. Drive is for architecture, research and interface specifications; milestone lists and completion tracking must not be independently maintained in Drive.

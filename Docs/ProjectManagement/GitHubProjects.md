# SIREngine GitHub Project — AI-first development, human-maintained future

**GitHub Projects V2 is the only project-management board.** GitHub Issues describe approved or proposed work, pull requests contain source changes, and GitHub Actions plus repository scripts keep project metadata consistent. No SQLite or outside coordination service is required.

## Authority and meaning of fields

The human owner decides architecture, milestone revisions, implementation scope and integration. A new issue, tracking label, priority field, or board status **does not grant authority**. Approved implementation scope requires a recorded owner decision for an exact plan revision and enumerated work-package IDs. A PR merge requires a separate decision tied to the exact head SHA and destination base, or an explicit owner-authored standing policy.

GitHub board fields describe **workflow metadata**; the Marathon worker must check the authenticated original approval receipt and dependency closure before claiming work. Team members may edit project presentation fields, issue forms and reports without changing this authority boundary.

## Configuration: activate the existing SIREngine project

The GitHub connector used to produce this patch has no Projects V2 GraphQL/board access, so the following activation must be completed with project credentials. The automation **never creates a new project** and refuses ambiguous project titles.

1. Review PR #280 and merge its exact accepted revision into the default branch **main**. Project workflow files must be on the default branch to handle repository events.
2. In repository Settings > Secrets and variables > Actions, configure the secret **SIR_PROJECT_TOKEN**. For this user-owned project, a personal access token (classic) with the GitHub **project** scope is the documented route; grant appropriate repository permissions as needed. Never write the token to an issue, source file or project field. GitHub's default GITHUB_TOKEN cannot mutate Projects V2.
3. Optional: set repository variable **SIR_PROJECT_NUMBER** from the existing project URL (for example, /users/SIGAAMDAD/projects/N). With no number, the script resolves the exact title **SIREngine**, failing if that title is ambiguous.
4. In Actions, run **SIREngine project operations** with operation **provision-labels**, first preview (apply false), then apply true. This adds missing label definitions without applying any approval label to an issue.
5. Run operation **provision**, first preview and then apply. It creates missing fields and saved views on the **existing** SIREngine project; it never alters an existing field type or overwrites an existing saved view.
6. Run operation **backfill**, first preview and then apply (limit 0 means all). This adds all repository issues and PRs carrying **marathon:tracked**, including historical closed records, then initializes recognized metadata. Backfill is idempotent.
7. Review the **hierarchy** preview. It only proposes native sub-issue links when an issue body explicitly identifies a Canonical parent or Parent issue. Apply only after reviewing the listed exact parent/child pairs. Existing conflicting parents are preserved.
8. Verify a fresh issue and PR are added and updated automatically by the repository-side project workflow.
9. Optionally enable GitHub Projects' built-in **Auto-add to project** for repository SIGAAMDAD/SIREngine with filter **is:issue,pr label:"marathon:tracked"** as a fallback. GitHub built-in Auto-add does not retroactively add old items; the backfill operation above covers history.

The workflow is triggered for issue/PR lifecycle changes and runs a weekday audit. With the project token absent, it issues a warning rather than claiming project synchronization succeeded. A separate intake workflow automatically applies **marathon:tracked** to new issues and PRs.

## Provisioned project fields

The provisioner adds missing fields only. It leaves GitHub's native **Status** field and any existing team fields intact.

| Field | Type | Usage |
|---|---|---|
| Delivery Stage | Single select | Proposed, Approved, Ready, In progress, In review, Verification, Awaiting merge approval, Blocked, Done, Closed without merge |
| Priority | Single select | P0, P1, P2, P3 from explicitly applied priority labels |
| Subsystem | Single select | Core, Renderer, GRDG, Shaders, OpenGL, DirectX, Vulkan, Meshing, Terrain, Physics, Audio, Input, Scripting, Console, Editor, World, Networking, QA, Other |
| Work Type | Single select | Roadmap, Milestone, Implementation, Bug, Verification, Documentation, PR |
| Pipeline Era | Single select | Fixed function, Programmable, Modern, Cross-era |
| Plan Revision | Text | Explicitly documented plan revision |
| Worker ID | Text | Assigned worker or human maintainer |
| Target Branch | Text | PR base or manually assigned implementation target |
| Integration Wave | Text | Owner-defined merge wave |
| Coverage (%) | Number | Verified coverage reported by reviewer; never guessed from issue closure |
| Target Date | Date | Owner/team planning target, never an inferred deadline |

Subsequent event synchronization follows repository state and metadata labels. **marathon:manual-stage** preserves a human-maintained Delivery Stage. Nonterminal work type and subsystem classification can also be edited by team members. A closed unmerged PR is not counted as merged or successful delivery.

## Provisioned views

| View | Layout | Intended use |
|---|---|---|
| Master Backlog | Table | All tracked repository work |
| Intake | Table | Proposed issues and review intake |
| Roadmaps | Roadmap | Roadmap parents |
| Milestones | Roadmap | Milestone parents and target dates |
| Ready for AI | Table | Currently marked ready tasks |
| Active Development | Board | Open work grouped by delivery stage |
| PR Review | Table | Open pull requests |
| Verification | Table | Independent QA work |
| Blocked | Table | Issues awaiting prerequisites |
| Completed History | Table | Closed issues and PRs |

The GraphQL provisioning creates views with layouts and initial filters. Configure grouping, column visibility, roadmap date fields, sorting and chart appearance in the GitHub Projects UI; the supported API does not set all these UI presentation settings.

## Recommended native board presentation

- Group **Active Development** by Delivery Stage and sort by Priority, then issue number.
- Display Subsystem, Priority, Work Type, assigned maintainer and linked PRs on the Master Backlog.
- Use **Target Date** for the roadmap timeline. Unscheduled milestones remain unscheduled rather than receiving fabricated dates.
- Enable GitHub's built-in **Issue closed** and **PR merged** status workflows, after confirming they do not conflict with your preferred Status field.
- Configure Insights charts in the UI for current item breakdown by Delivery Stage/Subsystem, issues closed per week, PR merges per week, open blocked items and workload by maintainer. Historical engine implementation percentage is not derivable from GitHub issue counts.

The weekday **report** run publishes downloadable **metrics.json** and **summary.md** artifacts and an Actions job summary. It reports issue/PR counts and recent activity. This is factual repository activity, not test coverage or engine completion.

## Lifecycle, AI dispatch, and cross-chat coordination

1. A planning chat creates or revises a roadmap/milestone with explicit work-package keys and acceptance criteria. **marathon:tracked** enrolls it; its scope is proposed until owner review.
2. The owner approves a revision and package allowlist with an auditable GitHub receipt. The director decomposes that scope into bounded child issues and checks dependencies.
3. The director applies **marathon:ready** only when prerequisites and write ownership are clear. A worker independently checks owner approval before taking a task.
4. Workers use separate branches, preserve exact source SHAs, update GitHub issue/PR checkpoints and hand work to independent QA.
5. The integrator merges only exact-revision authorized PRs with required current-SHA evidence. The accepted issue is then closed, and parent issue progress is reconciled.
6. A resumed or replacement worker reads current GitHub issue/branch/PR/claim state; no ChatGPT conversation is automatically awakened by a GitHub event.

GitHub issue assignment and labels do **not** provide atomic distributed locks. The Marathon coordination policy may use per-issue claim files on a separate GitHub control branch and must serialize conflicting source ownership. That branch must never merge into engine code.

## Team handover

Repository forms cover milestones, work packages, verification and defect reports. CONTRIBUTING.md and CODEOWNERS define how the future human team uses the same procedures. AI and human submissions receive the same source review and verification.

Project admins can reorganize views, charts, optional fields and release cadence without changing issue IDs, PR provenance or approval history. Changing approved architecture or package scope still requires another owner decision.

## Verification and limitations

- Code for the board operator and offline tests: Tools/ProjectManagement.
- Workflow syntax and offline tests: **Project automation validation**, automatically run on PRs affecting this system.
- The project-specific token and actual board activation are not available to the connected GitHub app; successful repository CI is not evidence of live Projects V2 mutation.
- The setup script does not create or modify custom Insights charts, board grouping configurations, native Project automation definitions, or a new project object. The listed UI configuration remains an explicit project-administration step.
- Exact project membership, field/view creation, backfill, and live webhooks must be verified by an authorized project run after deployment.

## Historical intake

As of the initial 2026-11-04 audit, 239 issues and 40 PRs were labeled **marathon:tracked** without altering their status. Newer intake issues and PRs are also tracked. The backfill operator recomputes the current complete inventory from GitHub rather than relying on that snapshot.

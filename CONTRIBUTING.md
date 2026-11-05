# Contributing to SIREngine

SIREngine's initial development is AI-created and manually directed. The same **GitHub Project, Issues, PRs, review rules and source evidence** are designed for a future human-maintained team.

## Before starting

1. Read the current `develop` branch, subsystem documentation and [GitHub Project operating guide](Docs/ProjectManagement/GitHubProjects.md).
2. Reuse an existing issue. Use a roadmap/milestone issue form for new architecture proposals, and implementation or verification forms for bounded work.
3. The human owner decides architecture, plan revisions, implemented scope and integration. A label, project column, or CI success is not approval by itself.
4. Manual workers should use the [Manual Worker Quickstart](Docs/ProjectManagement/ManualWorkerQuickstart.md). Select only an eligible task within the explicit owner-authorized selection set; check dependencies, active claims, existing branches, and path ownership.
5. Post a GitHub claim before editing. Concurrent workers use the existing GitHub-only `ops/sirengine-control` claim branch; there is no external scheduler or required SQLite database.
6. Develop on a dedicated branch, preserve source ownership, run source-specific checks, and attach exact SHA build/test/coverage evidence to a linked PR.
7. Independent QA records what was run and what was NOT_RUN. Integration requires separately recorded owner approval of the exact PR head and destination branch.
8. After verified integration, update and close the corresponding task and any fully accepted parent milestone. Leave unfinished work with a durable issue/PR checkpoint.

## Standards and review

- SIREngine follows C++17, repository-specific formatting, shared resource ownership/GRDG contracts, and explicit ABI gates. Inspect current source before relying on older planning documents.
- Do not commit generated build trees, SDK redistributables, secrets or cache artifacts.
- CODEOWNERS identifies review responsibility; later contributors can be added through normal repository administration.
- Issue templates are located in `.github/ISSUE_TEMPLATE/`; PR checklist in `.github/PULL_REQUEST_TEMPLATE.md`.

## Project board

The SIREngine GitHub Project is the sole task/milestone tracking system. Technical architecture and research may remain in Google Drive, with links to canonical GitHub milestones. All board curation can be done from native GitHub UI and the connected GitHub Issue/PR tools. There is no mandatory project-management script, scheduled job, or custom GraphQL token.

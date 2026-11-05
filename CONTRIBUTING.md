# Contributing to SIREngine

SIREngine is initially AI-created and will transition to a team-maintained development process. GitHub Projects is the shared planning and delivery interface. The owner remains the final authority for architecture, implementation scope, and integration.

## Starting work

1. Check the SIREngine Project and existing issues first. Reuse an issue rather than creating a duplicate roadmap.
2. For proposed designs, use the architecture milestone issue form. Marking an issue tracked does not approve the design.
3. Implementation issues refer to an explicit owner-approved plan revision, parent issue, work-package key, dependencies and owned writable paths.
4. Claim an eligible issue under the active coordination policy before editing shared source.
5. Create a branch scoped to the issue, commit focused source and tests, and open a PR linked to the issue.
6. Record actual checks, source SHA, missing platform/hardware evidence, and independent QA where required.
7. Integration requires the owner's approval of the exact PR revision and target branch, followed by the required checks.

## AI and human collaboration

AI-authored issues and code follow the same review and evidence rules as human-authored work. GitHub records the canonical state. Human contributors can use the same issue forms, project views, branch ownership records, and CI gates without adopting an AI-specific tool.

There is no automatic architectural approval and no automatic source merge arising from a project status, label, or passing CI result.

## Technical conventions

Read the current instructions, current branch and relevant subsystem documentation. SIREngine uses C++17, shared allocator/resource ownership, explicit driver contracts, and repository-specific formatting/test rules. The current source controls over stale roadmap summaries. Do not commit generated build trees, installed SDKs, secrets or CI cache artifacts.

## Project operations

The board provisioning, automation and team handoff process is documented in Docs/ProjectManagement/GitHubProjects.md. The project operations workflow supports owner-run setup, backfill, status synchronization and workload audit reports. Its project mutation credential is provided by repository maintainers; it is never stored in source.

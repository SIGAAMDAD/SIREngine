# Agent execution

SIR Director classifies each task as read-only inspection, planning, implementation, port, integration or benchmark. For editing, resolve branch/SHA, dirty tree, target build and acceptance checks; delegate by [Roles](Roles.md).

Small edits use one specialist directly. Cross-system work uses mutually exclusive WRITE sets and locked API boundaries. Integrate only approved changes, then run cumulative verification. Independently review high-risk diffs; don't claim unrun platform/hardware tests passed.

After first acceptance, each writing specialist follows [ImprovementLoop](ImprovementLoop.md), default one measurable experiment. Do not carry out unrelated enhancements, unlimited loops or unauthorized Git actions.

Report branch/SHA, changed paths, exact commands with PASS/FAIL/NOT RUN, measured improvements and outstanding gaps.

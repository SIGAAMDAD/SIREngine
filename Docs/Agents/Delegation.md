# Worker handoff contract

Use a single compact record:

```text
TASK: <bounded objective>
BASELINE: <branch + exact SHA; dirty files noted>
OWNER: <specialist>
READ: <relevant paths/context references>
WRITE: <exclusive file paths or NONE>
LOCKED: <shared public ABI, serialized formats, ownership, thread boundaries>
CHECKS: <target build, tests, negative cases, measurement metric if any>
AUTHORITY: <no branch/commit/push/merge unless explicitly approved>
RETURN: <changed paths, commands, exits, defects, improvement deltas, unresolved work>
```

Only the Director assigns shared headers and resolves collisions. Read-only research/review may overlap. The specialist may delegate once to a read-only researcher or verifier when useful; avoid recursive or competing writer subagents. All specialists use [ImprovementLoop](ImprovementLoop.md) after task acceptance.

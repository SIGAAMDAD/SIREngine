# Bounded worker contract

Every delegated task must include:

```
TASK: <one objective>
BASELINE: <branch and immutable commit SHA>
READ: <paths/entrypoints to inspect>
WRITE: <exclusive files or NONE>
LOCKED: <public APIs, ABI, serialization, ownership, threading constraints>
DEPENDENCIES: <other workers/interfaces and order>
CHECKS: <target compile, tests, formatting, negative cases>
AUTHORITY: no branch/commit/push/merge/issue-close unless specifically approved
RETURN: files, reasoning, exact commands, exits, failures, known gaps
```

- No two writing subagents may own the same file, generated artifacts, shared header or build script simultaneously.
- If a worker discovers it must edit outside `WRITE`, stop and escalate to director for redistribution.
- Integrator must re-evaluate contracts from actual source; independent workers are not their own verification authorities.
- Worktrees or branch isolation can be used **only when explicitly authorized**, and their creation is never implied by a request to delegate.
- Read-only subagents can investigate concurrently without extra worktrees when they do not modify the tree.
- The director remains responsible for cumulative test coverage and reconciled, single-tree acceptance.

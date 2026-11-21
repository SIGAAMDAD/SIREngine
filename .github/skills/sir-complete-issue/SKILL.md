---
name: sir-complete-issue
description: Complete an authorized SIREngine issue end to end with source inspection, bounded implementation, regression tests and evidence. Use when asked to complete an issue or milestone, not merely plan it.
argument-hint: '[issue number or task identifier]'
disable-model-invocation: true
---
# Complete issue

This workflow is controlled by the user-selected SIR Director or general coding agent. Read [the operations guide](../../../Docs/Agents/Operations.md) and `AGENTS.md`.

1. Resolve the issue requirements from the connected GitHub source if available, or supplied task description. Inspect exact checkout branch/SHA, `git status` and current source.
2. Define acceptance criteria, affected files, API/ABI ownership, runtime invariants, and negative tests. Do not create a new branch unless explicitly requested.
3. Delegate bounded source research and separate verification; parallel implementation only when file sets are disjoint and baseline/contracts frozen.
4. Implement **all authorized requirements**, compile the actual affected targets and fix test-discovered defects. If an external validation gate is inaccessible, continue what can be completed and mark that gate NOT RUN.
5. Reconcile source changes, focused tests, formatting and any required AST, sanitizer or benchmark gates.
6. Commit/push/close issue only when explicitly authorized by the user. Verify resulting state after each authorized mutation.
7. Return the [receipt template](../../../Docs/Agents/Receipts.md) filled with real evidence.

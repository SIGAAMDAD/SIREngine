---
name: sir-integration
description: Reconcile multiple SIREngine worker patches against a frozen baseline without unauthorized Git history changes or branch creation.
argument-hint: '[baseline SHA and worker outputs]'
---
# Integration

Read [the delegation contract](../../../Docs/Agents/Delegation.md).

1. Verify input SHAs, source ownership, patch contents, author provenance and test receipts.
2. Reject cross-worker overlap, unexpected generated files and incompatible public API edits before application.
3. Integrate approved changes in dependency order, fixing conflicts explicitly rather than overriding unrelated work.
4. Rebuild aggregate targets and run cross-system regression tests; inspect final diff and Git status.
5. Return exact integrated SHA or working-tree state, receipts and outstanding defects. Commit/push only with explicit permission.

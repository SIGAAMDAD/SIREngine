---
name: sir-investigate
description: Investigate SIREngine source behavior, call graphs, contracts, allocator or GPU lifetime issues without editing. Use for broad research that would pollute the parent agent context.
argument-hint: '[subsystem or defect]'
context: fork
---
# Read-only investigation

1. Identify checkout SHA, relevant entrypoints, consumers and tests.
2. Retrieve context with available search/cache tools, then verify against the actual source tree. Do not accept cached line numbers as current truth.
3. Trace ownership, initialization, update order, shutdown, error paths and invariants; note contradictory contracts.
4. Return a compact source-grounded report with exact paths, functions, evidence and suggested negative tests. Explicitly distinguish unconfirmed hypotheses.
5. Do not edit, branch, commit or run unrelated commands.

Use [the finding format](../../../Docs/Agents/Receipts.md).

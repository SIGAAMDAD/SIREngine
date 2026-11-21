---
name: sir-architecture-plan
description: Produce an executable source-grounded architecture plan or multi-workgroup ownership split for SIREngine with exact contracts and acceptance gates.
argument-hint: '[subsystem, migration, or workgroup scope]'
context: fork
---
# Architecture planning

Read `AGENTS.md` and [the delegation contract](../../../Docs/Agents/Delegation.md).

1. Inventory current files, symbols, APIs and source baseline; identify implemented behavior vs proposals.
2. State objectives, invariants, ownership/lifetimes, threading, memory and any public ABI changes.
3. Divide work only at file-safe seams. Give each workgroup owned files, immutable interfaces, required tests and integration order.
4. Enumerate negative/failure tests, compiler variants, migration/cutover rules and rollback boundaries.
5. Challenge the plan for hidden cross-team dependencies, missing consumer migrations, data races and untestable claims.
6. Output explicit issues and a dispatch verdict. Do not edit code or treat a plan as a tested implementation.

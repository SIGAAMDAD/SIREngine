---
name: sir-refactor
description: Refactor SIREngine owned C++ while preserving behavioral, ABI, lifetime and source ownership contracts; appropriate for multi-file structural cleanups.
argument-hint: '[subsystem and refactor objective]'
---
# Controlled refactor

1. State behavior to preserve, performance constraints, ownership graph, APIs/ABI, serialized formats and test baseline.
2. Inventory affected code including indirect consumers; check existing project architecture documents.
3. Make the smallest cohesive steps, preserving user-requested behavior. Do not combine unrelated modernization or namespace/container migrations.
4. Update and run equivalence/negative tests, compiler checks and performance comparisons if the change affects hot paths.
5. If outputs differ, identify whether expected, accepted or regression; do not silently change semantics.
6. Report exact diff, validation, compatibility gaps and future stages separately.

---
name: sir-verify
description: Independently inspect SIREngine change correctness, test coverage, compiler validity, ABI and memory/threading safety, returning evidence and explicit gaps.
argument-hint: '[diff, branch, subsystem or commit]'
context: fork
---
# Independent verification

Read [the verification matrix](../../../Docs/Agents/Verification.md). Verify changes from the actual checkout and diff; do not rely on the implementation agent's success narrative.

1. Enumerate changed code, dependencies and applicable acceptance gates.
2. Check memory lifetime/allocator routing, thread affinity, exception/error handling, ABI/layout, serialization and platform coverage.
3. Run read-only build/test/analyzer commands when available. For C/C++ qualification, use actual compilation commands and Clang AST where required.
4. Document negative cases and reproduce failures as possible.
5. Return gate-by-gate PASS/FAIL/NOT RUN and prioritized defects with file/line, evidence, reproduction and owner action.

No source edits, branch creation or GitHub mutations.

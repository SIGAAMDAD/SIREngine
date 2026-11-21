---
name: sir-implement
description: Implement a bounded SIREngine feature or bug fix against a known source baseline, with tests, changed-file formatting and exact results.
argument-hint: '[scope and optional owned paths]'
---
# Implementation

Read `AGENTS.md`, matching `.github/instructions` files and actual CMake target settings.

1. Confirm baseline SHA, source ownership, current changes and user-approved scope.
2. Inspect callers, lifetimes, error handling, build flags, tests and the applicable engine convention documents.
3. Edit only assigned paths. Keep APIs/ABI/serialized data stable unless the change explicitly requires migration.
4. Add meaningful normal and negative tests; compile/test the actual target. Format changed first-party C++ using repository config.
5. Return a [receipt](../../../Docs/Agents/Receipts.md): exact modifications, commands and exits, failed gates and unresolved integration obligations.

Do not create a branch or publish changes without explicit authorization.

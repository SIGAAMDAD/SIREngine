# Container Convention Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate every `Source/Core/Containers/*.h` to SIREngine's actual C++ conventions without changing public APIs or behavior.

**Architecture:** Keep the current container types, algorithms, allocator contracts, and header-only template model. Apply mechanical convention changes in place, moving only non-template definitions when this does not change linkage or API; retain compatibility exceptions where strict style would alter caller source behavior.

**Tech Stack:** C++23 with C++17 fallback, clang-format 23.1.5, CMake/Ninja standards gates, existing SIREngine container tests.

**Spec:** `docs/superpowers/specs/2026-11-24-containers-convention-migration-design.md`

## Global Constraints

- Public APIs and source compatibility must not change.
- Preserve allocator, lifetime, invalidation, exception/failure, and aliasing behavior.
- Use canonical `__SIR_*_H__` guards and `#include <Core/SIRPCH.h>` in first-party headers.
- Keep global-namespace declarations, `m_` class fields, prefixed semantic names, fixed-width engine integers, and explicit result/error contracts.
- Do not modify unrelated existing user changes.

## Review Focus

- Header-only template visibility: compile all affected headers through authentic targets and header checks.
- Special-member/source compatibility: retain implicit conversion behavior where required by existing callers.
- Failure rollback and allocator ownership: run the existing container tests and policy/tidy checks.
- C++17 fallback: run the configured compatibility gate.
- Scope control: review the final diff for changes outside containers and plan/spec documents.

### Task 1: Baseline and inventory

**Files:**
- Read/verify: all `Source/Core/Containers/*.h`
- Read/verify: `.clang-format`, `Docs/Standards/cpp.md`, standards tools
- Modify: plan ledger under `.superpowers/sdd/`

- [ ] Record current branch/worktree state and available build presets.
- [ ] Run focused format/policy/header checks or their direct script equivalents to establish the baseline.
- [ ] Classify violations into mechanical header/format changes, declaration changes, and compatibility-sensitive cases.

### Task 2: Mechanical header and formatting migration

**Files:**
- Modify: every affected header in `Source/Core/Containers/`

- [ ] Normalize license/PCH/guard structure without changing declarations.
- [ ] Apply the pinned root clang-format to all container headers.
- [ ] Re-run format and header checks; repair any self-containment or include-order regressions.

### Task 3: Declaration and policy migration

**Files:**
- Modify: container headers with policy findings
- Modify: dependent `Source/` call sites only if required to preserve compilation after an API-compatible declaration correction

- [ ] Normalize class declaration order, special-member declarations, qualifiers, annotations, and naming where the change is source-compatible.
- [ ] Preserve compatibility-sensitive constructors and public signatures; record intentional exceptions in the ledger.
- [ ] Replace prohibited constructs only when the equivalent preserves observable behavior and allocation/lifetime semantics.
- [ ] Run focused compile/tests after each logical batch.

### Task 4: Full verification and review

**Files:**
- Read/verify: final container diff and dependent usage

- [ ] Run `sir-format-check`, `sir-policy-check`, `sir-header-check`, `sir-tidy-check`, `sir-compatibility-check`, and relevant build/test targets when configured.
- [ ] Run the full available test suite/build for the checkout.
- [ ] Review `git diff --check`, API signatures, and changed-file scope.
- [ ] Fix only verified failures, then repeat the affected checks.


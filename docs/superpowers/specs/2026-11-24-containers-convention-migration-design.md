# Container Convention Migration Design

## Intent

Bring every header in `Source/Core/Containers` into compliance with the
repository's actual first-party C++ conventions without changing container
behavior, public semantics, allocation contracts, or supported language modes.

## Goals

- Apply the canonical SIREngine header structure: MPLv2 notice where required,
  `Core/SIRPCH.h`, canonical `__SIR_*_H__` guards, and annotated closing guards.
- Normalize declarations to the standards in `Docs/Standards/cpp.md`: global
  namespace, member naming, fixed-width engine types, explicit special members,
  constructor/destructor grouping, `[[nodiscard]]` on relevant results, and
  out-of-class ordinary method definitions where practical.
- Apply the pinned root `.clang-format` consistently to all container headers.
- Preserve APIs and runtime behavior except for source-level correctness fixes
  that are directly required by the stated conventions.
- Validate with the repository's formatting, policy, header, compatibility,
  tidy, build, and test gates available in the current environment.

## Non-goals and anti-goals

- No redesign of container algorithms, growth policies, allocator behavior, or
  invalidation rules.
- No unrelated changes outside `Source/Core/Containers` and the migration
  design/plan documents.
- No broad modernization solely for newer C++ features; C++17 fallback support
  remains intact.
- Do not hide violations with formatter exclusions or policy suppressions.

## Constraints

- Existing user changes in the worktree must be preserved.
- Public header self-containment and template definitions must remain valid.
- The repository's global-namespace and `SIR` naming conventions remain the
  authority for these headers.
- Changes must be reviewable by file and validated by fresh command output.

## Approaches

1. **Formatting-only:** apply clang-format. This is low risk but leaves header,
   declaration-layout, ownership, and semantic policy violations unresolved.
2. **Gate-driven repair:** fix only current checker findings. This is mechanical
   but incomplete because the checker does not encode every written convention.
3. **Full standards migration (selected):** audit every container header,
   normalize structural and semantic conventions while preserving behavior,
   then use the gates to catch regressions. This best matches the request that
   each container follow the actual conventions.

## Implementation outline

1. Inventory each header's guard, license/PCH structure, declarations, special
   members, result annotations, and formatter state.
2. Make mechanical source changes in small batches, keeping public signatures
   compatible and preserving intentional low-level implementation details.
3. Add or update focused tests only where a convention correction could alter
   observable behavior; otherwise rely on existing container tests and compile
   coverage.
4. Run formatting and standards checks, then build and test the configured
   targets. Review the final diff for scope and accidental behavior changes.

## Open builder decisions

- Whether ordinary in-class method bodies can be moved out of headers without
  harming template visibility or forcing broad explicit-instantiation changes.
- Which existing exceptions are intentional and should be documented rather
  than mechanically changed.
- The exact available build preset/target set in this checkout.


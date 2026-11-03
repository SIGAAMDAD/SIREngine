# ADR-0001: Pin the existing Godot fork during SIREngine bootstrap

- **Date:** 2026-11-02
- **Status:** accepted for bootstrap; superseded by future explicit monorepo migration

## Decision

Remove the obsolete custom SIREngine source layout from the new `develop` tree. Pin `SIGAAMDAD/godot` at a verified commit as `engine/godot`, use pinned EASTL family submodules, and create SIR-owned headers and infrastructure beside the engine source. Maintain the old develop commit in Git history for traceability.

## Reasoning

The user's actively maintained Godot fork is the authoritative base for the editor/backend changes. Keeping its exact commit avoids silently transplanting mismatched engine sources or inventing missing generated bindings. The SIREngine repository must evolve into a deeply modified Godot descendant, so the Godot code needs a reproducible origin while the great allocator/container/resource migration is carried out.

## Consequences

SIREngine is not yet a fully consolidated Godot source monorepo. Any source patches in `engine/godot` must be committed in that source repository and the pinned SHA updated here until the planned source consolidation. CMake owns SIR infrastructure; SCons still builds the Godot engine. Third-party license provenance is kept.

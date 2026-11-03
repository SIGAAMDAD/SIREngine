# 13 — Source and backend migration sequence

**Status:** phased implementation plan.

The present repository baseline pins the existing Godot fork as a submodule. That is a reproducible source origin for the editor and renderer while SIR-owned modules are introduced. It is **not** a completed import of Godot's tracked source into a native SIREngine monorepo. Transition choices and provenance are in [ADR-0001](../adr/0001-source-strategy.md).

## Dependency-ordered phases

1. **Source authority:** inventory the exact Godot fork, module registrations, generated bindings, build variants, submodules and licensing; establish compiler databases and baseline editor/runtime tests.
2. **Allocator domain trace:** instrument every Godot, Jolt, third-party and editor allocation path. Reconcile NomadFramework/NMDMemory sources against their exact source archive and original history before importing applicable implementations; measure retained pages and fallback allocations.
3. **Container migration:** introduce SIR allocators for EASTL, migrate selected hot structures and central Godot allocation boundaries, preserve Godot ABI, and use actual Clang USR/AST callsite analysis.
4. **GRDG:** add canonical identity/handle registry, dependency graph, immutable publication, epochs, residency policy and read views. Migrate consumer systems in bounded slices.
5. **Cooked runtime:** introduce stable content IDs, deterministic cooked formats, scene/asset conversions and streaming, then remove unnecessary shipping Node representation from static content.
6. **Provider backend integration:** migrate rendering, physics, streaming, audio and job allocators/ownership; modern OpenGL 4.6 and Vulkan 1.4 require explicit feature implementation. FMOD first; Wwise/OpenAL later.
7. **Game adapters:** build The Nomad and Sigilbound against versioned SIR interfaces, with profiling, editor regression scenes and representative performance workloads.
8. **Source consolidation:** import or restructure Godot code into the SIREngine repository with documented provenance and license preservation only after intermediate integrations are reproducible. Replace the submodule and SCons bridge in a separately verified patch if native CMake is the accepted build system.

## Migration gates

Each phase must compile and have an exact per-file contract, source-hashed validation, rollback plan and resource/memory measurements. No broad all-at-once identifier rename. A future source import is a source migration, not a `.gitmodules` edit.

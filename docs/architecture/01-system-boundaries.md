# 01 — Layers, ownership and dependencies

**Status:** approved target layout.

```text
Game project (The Nomad / Sigilbound / others)
    -> public SIR APIs, asset schemas, gameplay integration hooks
Editor/importer/cooker (Godot-derived authoring systems)
    -> SIR asset schemas, resource handles, diagnostics
SIR core (memory, containers, GRDG, jobs, platform)
    -> graphics / physics / audio / networking provider boundaries
Backend providers (Vulkan, D3D12, OpenGL, FMOD, etc.)
    -> operating system and vendor SDKs
```

The common layer cannot depend on gameplay classes or editor UI types. Providers may depend on common services but may not allocate an independent resource registry for the same SIR identity. Game modules select policies and capacities without changing general engine ownership semantics.

## Source tree

`include/sir/` contains public headers. `src/core/` will implement common memory/jobs/handles; `src/resources/` owns GRDG and streaming; `src/rendering/`, `src/audio/`, `src/physics/`, `src/platform/` house provider implementations; `src/editor/` and `src/cooker/` hold integration and cooking code. `engine/godot/` is a pinned source checkout during the transition. `third_party/` is for externally versioned dependencies. `tests/` contains unit, integration, stress and scene tests.

## APIs and invariants

- Public handles are typed, versioned and cheap to copy; direct owned pointers do not cross module lifetime boundaries.
- Cross-system payload access uses canonical GRDG identity and a scoped read view, not an independently loaded duplicate.
- Errors cross subsystem boundaries as explicit results/status objects; no hidden allocations in fatal-reporting paths.
- Lifecycle order: allocator bootstrap → platform/jobs → resource registry → backend providers → editor/game; shutdown in reverse, with outstanding read epochs reclaimed before resource teardown.
- Serialization, ABI and SDK provider versions are tracked independently. A new backend must not change cooked resource IDs.

## Integration rule

New source first enters `src/` with tests. The relevant Godot-facing adapter is introduced using its exact fork API; an upstream file modification requires separate source- and build-specific verification. Avoid broad renames until ownership and compilation closure have been proved.

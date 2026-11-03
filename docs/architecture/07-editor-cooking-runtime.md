# 07 — Editor, asset cooking and shipping runtime

**Status:** architectural contract, not an implemented cooker.

SIREngine preserves Godot's editor nodes, inspectors, importers, scene serialization and authoring tools. Shipping static world content is transformed into compact, versioned cooked records so that runtime residency does not carry per-object Node/Resource bookkeeping for static assets that need no runtime behavior.

## Pipeline

```text
Godot scenes / materials / imports
    -> source graph and dependency extraction
    -> deterministic cooking + validation
    -> asset catalog / stable IDs / dependency graph
    -> chunked, versioned platform packages
    -> streaming loader -> GRDG -> provider-specific views
```

Cooked records must include schema version, stable asset identifier, local offsets, bounds, dependency IDs, alignment requirements and integrity checks. Preserve explicit mapping back to editor scene paths for diagnostics and hot reload; never derive runtime IDs from unstable pointer addresses.

## Static/dynamic boundary

Authored static props, scenery, terrain regions, collision BVHs, navigation tiles and HLOD groups are candidates for baked arrays. Dynamic entities retain entity/state lifetimes and gameplay hooks. Instance transform streams should be stored compactly, ordered for spatial locality, and consumed by rendering/physics/navigation without redundant CPU mesh copies.

## Reliability

The cooker must be deterministic for the same source and tool versions. Generation of shader/material variants, GPU-friendly geometry and resource dependency closure happens offline whenever practical. Runtime code validates version, bounds, offsets and allocations before publishing a resource. Failed or corrupted cooked chunks must not corrupt the registry.

## Acceptance

Round-trip editor import/save for representative projects; scene-versus-cooked visual/physics equivalence tests; deterministic package hashes; streamed loading/unloading with memory traces; backward-compatible version migration rules; and failure-injection tests for partial I/O, missing dependencies and interrupted streaming.

# Milestone 3 — Port Terrain3D to SIREngine

**Status:** not started. **Source reference:** `Upstream/Terrain3D` at the pinned fork commit.

Integrate the terrain editor and runtime into the unified engine. Preserve the fork's established editor tools, region serialization, brush operations, material authoring, instancers, collision and erosion workflows. Source changes should preserve the behavior of the working fork until equivalent functionality is verified in SIREngine.

## Deliverables

1. Exact Terrain3D source and shader inventory, backend dependencies and import/serialization compatibility contract.
2. Native terrain module CMake target and editor registrations, including complete shader-build requirements.
3. Runtime terrain-region streaming and existing clipmap/LOD, virtual texture and HLOD pathways brought into the common resource and allocation conventions as appropriate.
4. Brush editing, save/reload, collision, terrain rendering and streaming regression scenes on supported platforms.

**Exit gate:** terrain authoring and rendering function within the native editor/runtime, with saved data round-tripping and representative resource-performance measurements.

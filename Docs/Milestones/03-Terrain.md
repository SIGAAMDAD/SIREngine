# Milestone 3 - Integrate SIREngine Terrain

**Status:** planned.
**Ownership:** `Source/Terrain/` (built-in subsystem) and `Source/Terrain/Editor/` (specialized authoring).

Terrain is a **built-in engine system**, not a plugin or optional module. The milestone brings the existing, heavily modified terrain algorithms, authoring workflow and data contracts into first-party SIREngine source with a stable, engine-level API.

## Deliverables

1. Reconcile the actual terrain source and shaders, terrain-region serialization, editor tooling, and backend dependencies. Record exact source provenance and preserve required licensing.
2. Create native CMake source targets for the built-in Terrain runtime subsystem and its integrated editor functionality.
3. Preserve region storage, brush operations, painting, erosion, material authoring, instancing, collision, virtual textures, clipmap/LOD streaming and HLOD behavior in the applicable integration paths.
4. Connect terrain resource IDs, allocations and residency to the common engine resource and memory interfaces; avoid unrelated ownership copies.
5. Validate Windows/Linux editor operations, scene/region save-reload, shader compilation, terrain rendering, collision, editing, and runtime streaming.

**Exit criteria:** Terrain functions as a normal, built-in SIREngine subsystem with native build and runtime/editor regression evidence.

# SIRMeshCache standalone foundation (mesh-cache branch)

**Branch:** `feature/mesh-cache-foundation`, parent: `develop` `51666fadadf8112b3b2d5bf919ba11dde55d931b`.

## Current implemented slice

- Engine-owned C++17 library `SIR::MeshCache`, independent from Jolt, GRDG, Streaming, Terrain3D and graphics backends.
- Caller-provided fixed-capacity aligned slot memory; no heap allocations or GPU work in the library.
- Immutable canonical *static CPU triangle* source view (`float XYZ`, 32-bit indexed triangles), identified by asset/revision/variant key.
- Duplicate asset/revision keys rejected; generation-checked handles and non-owning geometry borrows; a single source pin is transferred to the cache upon insertion and released once after every borrow finishes.
- Two systems can receive the same physical position/index pointers without copying. Old revisions remain valid while borrowed; retiring sources reject new acquisitions.
- Explicit shutdown fails while borrows are outstanding. Reader and mutation entry points are single-thread-affine until the GRDG read-epoch integration is merged.

## Ownership contract

The caller supplies a long-lived immutable geometry source and a one-shot source-pin release callback. The cache owns the pin lifetime, never the source bytes, and never allocates or frees them. Physical CPU storage and long-lived readers belong to the future GRDG/NMD integration. GPU pools, compressed virtual geometry pages, physics BVHs, meshes with skinning, displacement cooking and GPU command generation are separate dependent implementation milestones.

**No original displacement texture is required or accepted by this interface.** Future offline displacement cooker emits final CPU positions and compressed GPU geometry pages with a shipping manifest that rejects displacement-source texture dependencies. See SIR-VG-DISP-001 plan.

## Acceptance

`ctest -R mesh.cache.foundation` verifies invalid input, one-copy memory identity across independent consumers, duplicate rejection, multiple borrows, deferral until last release, stale-generation protection, fixed capacity and shutdown discipline. The standalone test does not certify concurrent GRDG readers, Jolt physical sharing, GPU residency, HLOD or driver runtime support.

## Next source boundaries

1. Couple the source pin to the canonical GRDG provider lease, without introducing another owner of bytes.
2. Add immutable typed geometry views and cooked `SIRPack` displacement/LOD metadata, and verify shipping manifest excludes authoring heightmaps.
3. Add Jolt static triangle shape provider and GPU geometry page pool, each using the same canonical mesh identity.
4. Integrate GPUScene, HLOD, LOD and indirect rendering once the native ABI supports them.

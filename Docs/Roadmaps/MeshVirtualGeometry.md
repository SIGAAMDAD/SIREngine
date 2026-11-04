# SIR-VG-001 — Shared Mesh Cache and Virtualized Geometry Roadmap

**Canonical tracking issue:** [#36](https://github.com/SIGAAMDAD/SIREngine/issues/36)
**Implementation branch:** [`feature/mesh-cache-foundation`](https://github.com/SIGAAMDAD/SIREngine/tree/feature/mesh-cache-foundation) at original foundation commit [`9f0a566a8d9d`](https://github.com/SIGAAMDAD/SIREngine/commit/9f0a566a8d9d091ebee47e4d701dbffe1a2b5b8e).
**Base branch:** `develop` at `51666fadadf8112b3b2d5bf919ba11dde55d931b`.
**Status (2026-11-04):** R0 independent CPU cache slice committed; 35 roadmap implementation/acceptance issues open (29 original + 6 cross-backend additions). Remaining stages are requirements and executable plans, **not** implemented source.
**Source plan:** SIR-VG-001 (Nanite-inspired SIREngine-native virtualized geometry), SIR-VG-DISP-001 (baked displacement), existing `Docs/Architecture/20-mesh-cache-foundation.md`, and renderer/GRDG/Streaming design authorities.

## 1. Objectives and invariant boundaries

SIREngine will share immutable static source geometry across rendering, CPU physics/collision, BVH queries and many world instances. One canonical mesh identity/revision belongs to GRDG; CPU memory is physically allocated/accounted through SIRMemoryManager; `SIRMeshCache` owns logical geometry lookups/leases; `SIRRenderingDevice` alone owns GPU buffers and native queue submissions. Streaming demand is unified through World/Streaming, not another mesh-specific scheduler.

The independent CPU foundation in `Source/Mesh/SIRMeshCache.*` deliberately **does not depend** on unmerged GRDG, Jolt, Terrain3D or GPU drivers. Its published semantics currently cover caller-provided fixed-capacity slot backing, immutable borrowed source pointers, source-pin callback retirement, generation-safe handles and duplicate key rejection. It has native CMake and unit test targets. Earlier local validation reported 84.76% line and 71.63% branch coverage; **R0-01 is outstanding** and requires fresh exact-SHA results.

**Shipping displacement contract:** Static mesh height/displacement textures are authoring inputs only. Cook adaptive subdivisions and displaced final positions, optimize via meshoptimizer, produce compressed vertex/index/cluster/HLOD geometry and optional displaced collision, then ship *only* cooked data. A shipping SIRPack must contain no original displacement texture bytes, loadable source paths/IDs, runtime displacement sampler/binding, or transitive source dependency. Runtime uses precomputed LOD pages, without initial-scene or per-frame displacement/tessellation work for these baked assets.

**Backend contract:** one canonical cooked geometry hierarchy, resource identity, residency directory and complete-group LOD policy for EVERY graphics driver. The guaranteed common execution is CPU reference cut + conventional indexed drawing (GL1.2 uses legal client arrays). Vulkan/D3D12 indexed indirect, qualified GL4.6/D3D11 FL11 GPU execution, mesh/task shaders, compute software raster and RT are independently capability-gated accelerations. A legacy fixed-function backend must never depend on GPU compute, indirect, GLSL, FBO or a driver-specific culler. No Epic-licensed Nanite implementation/shaders copied; pin permissive meshoptimizer donors and license notices.

## 2. Stage index

| Stage | Workstream | Issues | Direct links |
| --- | --- | ---: | --- |
| R0 | Independent baseline and authority freeze | 2 | [#37](https://github.com/SIGAAMDAD/SIREngine/issues/37), [#38](https://github.com/SIGAAMDAD/SIREngine/issues/38) |
| R1 | Offline cooking, compressed clusters, no-source-map shipping | 4 | [#39](https://github.com/SIGAAMDAD/SIREngine/issues/39), [#40](https://github.com/SIGAAMDAD/SIREngine/issues/40), [#41](https://github.com/SIGAAMDAD/SIREngine/issues/41), [#42](https://github.com/SIGAAMDAD/SIREngine/issues/42) |
| R2 | CPU canonical memory, Jolt shapes, BVH and collision policy | 3 | [#43](https://github.com/SIGAAMDAD/SIREngine/issues/43), [#44](https://github.com/SIGAAMDAD/SIREngine/issues/44), [#45](https://github.com/SIGAAMDAD/SIREngine/issues/45) |
| R3 | Shared page residency, CPU derivatives, GPU pools and ABI | 4 | [#47](https://github.com/SIGAAMDAD/SIREngine/issues/47), [#48](https://github.com/SIGAAMDAD/SIREngine/issues/48), [#49](https://github.com/SIGAAMDAD/SIREngine/issues/49), [#268](https://github.com/SIGAAMDAD/SIREngine/issues/268) |
| R4 | Shared scene, cluster cuts, CPU batches and GPU execution | 7 | [#50](https://github.com/SIGAAMDAD/SIREngine/issues/50), [#51](https://github.com/SIGAAMDAD/SIREngine/issues/51), [#52](https://github.com/SIGAAMDAD/SIREngine/issues/52), [#269](https://github.com/SIGAAMDAD/SIREngine/issues/269), [#270](https://github.com/SIGAAMDAD/SIREngine/issues/270), [#271](https://github.com/SIGAAMDAD/SIREngine/issues/271), [#272](https://github.com/SIGAAMDAD/SIREngine/issues/272) |
| R5 | Streamed world geometry, terrain and HLOD integration | 3 | [#53](https://github.com/SIGAAMDAD/SIREngine/issues/53), [#54](https://github.com/SIGAAMDAD/SIREngine/issues/54), [#55](https://github.com/SIGAAMDAD/SIREngine/issues/55) |
| R6 | Material recipes, clutter and static object lifetimes | 3 | [#62](https://github.com/SIGAAMDAD/SIREngine/issues/62), [#64](https://github.com/SIGAAMDAD/SIREngine/issues/64), [#66](https://github.com/SIGAAMDAD/SIREngine/issues/66) |
| R7 | Skeletal entities and optional acceleration | 3 | [#67](https://github.com/SIGAAMDAD/SIREngine/issues/67), [#68](https://github.com/SIGAAMDAD/SIREngine/issues/68), [#69](https://github.com/SIGAAMDAD/SIREngine/issues/69) |
| R8 | Editor, performance and per-driver acceptance | 6 | [#70](https://github.com/SIGAAMDAD/SIREngine/issues/70), [#71](https://github.com/SIGAAMDAD/SIREngine/issues/71), [#72](https://github.com/SIGAAMDAD/SIREngine/issues/72), [#75](https://github.com/SIGAAMDAD/SIREngine/issues/75), [#76](https://github.com/SIGAAMDAD/SIREngine/issues/76), [#273](https://github.com/SIGAAMDAD/SIREngine/issues/273) |

## 3. Dependency DAG

```text
R0 contract #38 + CPU MeshCache #37
  +--> R1 cooker/SIRPack #39/#40 (+ baked displacement #41/#42)
  +--> R2 canonical GRDG/CPU mesh and collision #43/#44/#45
  +--> R3 common residency protocol #49 + CPU/legacy derivative #268
  |       +--> Modern-only sealed GPU pool #47 and indirect ABI #48
  +--> R4 CPU scene snapshot #50 + CPU cut/reference #51
          +--> Common draw-plan and sorted draw batches #269
                +--> Fixed function #270: GL1.2/1.5, D3D7/8
                +--> Programmable #271: GL1.5 ext, GL2.x/3.3,
                |                        D3D8/9/10/11 FL9/10
                +--> Modern indexed-indirect first image #52: Vulkan/D3D12
                +--> Modern added #272: GL4.6, D3D11 FL11
                         +--> Advanced optional #69 (mesh/task/RT)
  +--> R5 World/Streaming demand #53 + World Partition #54
          +--> Terrain3D HLOD #55
  +--> R6 materials / clutter / world objects #62/#64/#66
  +--> R7 skeletal/ragdoll optional #67/#68
  +--> R8 editor #70, benchmarks #71, modern GPU gate #72,
          optional R7 gates #75/#76,
          per-driver independent qualification #273
```

**Scheduling:** #38 contract freeze and R1 cooked hierarchy enable CPU/cook/core verification before any GPU compute implementation. #49 shares root/page-transaction semantics across CPU and GPU; #47/#48 are required only by drivers using those capabilities. #51 produces a CPU scalar oracle before any Hi-Z/compute lowering. #52 is Vulkan/D3D12 *GPU-tier* acceptance rather than a global prerequisite to fixed or programmable execution. R5/6 streaming and materials integrate through the common residency/frame plan without requiring every driver to ship. R8-06 (#273) certifies individual driver capability tiers and never reports a broad hardware PASS from another backend. All existing source ownership and issue-specific dependencies must be reconciled to this graph before implementation.

## 4. Implementation issues and acceptance gates

### R0 — Independent baseline and authority freeze

**Exit gate:** Standalone C++17 CPU cache is at 100% locally compilable line/branch/function coverage; exact GRDG, NMD, Streaming, driver and cooked schema contracts frozen before any cross-branch integration.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R0-01` | [#37](https://github.com/SIGAAMDAD/SIREngine/issues/37) | Complete CPU MeshCache coverage and fault testing |
| `VG-R0-02` | [#38](https://github.com/SIGAAMDAD/SIREngine/issues/38) | Freeze cross-system interface and ownership contracts |

### R1 — Offline cooking, compressed clusters, no-source-map shipping

**Exit gate:** Deterministic SIRPack schema and meshoptimizer hierarchical cluster cooking; adaptive static displacement baked before LOD; negative packaging tests reject *all* source displacement dependencies.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R1-01` | [#39](https://github.com/SIGAAMDAD/SIREngine/issues/39) | SIRPack immutable mesh and page manifest |
| `VG-R1-02` | [#40](https://github.com/SIGAAMDAD/SIREngine/issues/40) | Cluster/meshlet hierarchical LOD cooker |
| `VG-R1-03` | [#41](https://github.com/SIGAAMDAD/SIREngine/issues/41) | Offline adaptive heightmap displacement |
| `VG-R1-04` | [#42](https://github.com/SIGAAMDAD/SIREngine/issues/42) | No displacement-source textures in shipping packages |

### R2 — CPU canonical memory, Jolt shapes, BVH and collision policy

**Exit gate:** Identical static CPU vertex/index pointers safely shared across relevant physics and CPU BVH consumers; Jolt collision built and retired with exact revisions; no copying per repeated instance.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R2-01` | [#43](https://github.com/SIGAAMDAD/SIREngine/issues/43) | GRDG/NMD immutable mesh geometry leases |
| `VG-R2-02` | [#44](https://github.com/SIGAAMDAD/SIREngine/issues/44) | Shared static Jolt collision shapes and per-instance policy |
| `VG-R2-03` | [#45](https://github.com/SIGAAMDAD/SIREngine/issues/45) | CPU BVH/query, navigation and collision revisioning |

### R3 — Sealed GPU geometry page residency and driver ABI

**Exit gate:** complete-group generation-safe residency and root fallback across CPU and GPU with atomic publication/retirement; engine-budgeted CPU page derivatives. Sealed native GPU pools, indexed-indirect-count and cross-queue fences are separately admitted modern GPU capabilities.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R3-01` | [#47](https://github.com/SIGAAMDAD/SIREngine/issues/47) | Fixed GPU geometry page pool and directory |
| `VG-R3-02` | [#48](https://github.com/SIGAAMDAD/SIREngine/issues/48) | Native Vulkan/D3D12 indexed-indirect-count ABI |
| `VG-R3-03` | [#49](https://github.com/SIGAAMDAD/SIREngine/issues/49) | Dependency-complete common group/page publication and GPU derivative retirement |
| `VG-R3-04` | [#268](https://github.com/SIGAAMDAD/SIREngine/issues/268) | CPU/legacy page backing and native buffer/client-array derivatives |

### R4 — GPUScene, cluster traversal, first images

**Exit gate:** packed CPU scene instance snapshot with optional GPU mirror, reference cluster cut, deterministic draw plan, conservative frustum/cone/occlusion. Fixed/programmable execution and Vulkan/D3D12 screenshot/depth equivalence have independent native qualification gates; no mandatory Hi-Z on CPU driver tiers.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R4-01` | [#50](https://github.com/SIGAAMDAD/SIREngine/issues/50) | GPUScene instance tables and dirty journals |
| `VG-R4-02` | [#51](https://github.com/SIGAAMDAD/SIREngine/issues/51) | Hierarchical cluster culling and screen-space LOD |
| `VG-R4-03` | [#52](https://github.com/SIGAAMDAD/SIREngine/issues/52) | Initial Vulkan/D3D12 modern GPU indirect parity |
| `VG-R4-04` | [#269](https://github.com/SIGAAMDAD/SIREngine/issues/269) | Deterministic common complete-cut and material-sorted draw-plan compilation |
| `VG-R4-05` | [#270](https://github.com/SIGAAMDAD/SIREngine/issues/270) | Fixed-function CPU/client-array/buffer execution (GL1.2/1.5, D3D7/8) |
| `VG-R4-06` | [#271](https://github.com/SIGAAMDAD/SIREngine/issues/271) | Programmable-era CPU-batch execution (GL1.5 ext/2.x/3.3, D3D8/9/10/11 downlevel) |
| `VG-R4-07` | [#272](https://github.com/SIGAAMDAD/SIREngine/issues/272) | Additional qualified GPU execution (GL4.6, D3D11 FL11) |

### R5 — Streamed world geometry, terrain and HLOD integration

**Exit gate:** GPU feedback routes to sole World/Streaming owner; async SIRPack/page uploads are cancel-safe and fair; World Partition geometry/collision demand and Terrain3D HLOD region transitions validated for teleports.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R5-01` | [#53](https://github.com/SIGAAMDAD/SIREngine/issues/53) | Geometry feedback and single World/Streaming demand |
| `VG-R5-02` | [#54](https://github.com/SIGAAMDAD/SIREngine/issues/54) | World Partition geometry/collision activation |
| `VG-R5-03` | [#55](https://github.com/SIGAAMDAD/SIREngine/issues/55) | Terrain3D HLOD and clipmap boundary integration |

### R6 — Material recipes, clutter and static object lifetimes

**Exit gate:** Shadows/material/depth/velocity passes consume one geometry hierarchy; instanced clutter/prototype reuse plus selective dynamic promotion; buildings/occluders and carried/dropped weapons remain revision coherent.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R6-01` | [#62](https://github.com/SIGAAMDAD/SIREngine/issues/62) | Material, shadow, depth and compositor integration |
| `VG-R6-02` | [#64](https://github.com/SIGAAMDAD/SIREngine/issues/64) | Clutter instancing, wind and physics promotion |
| `VG-R6-03` | [#66](https://github.com/SIGAAMDAD/SIREngine/issues/66) | Boulders, buildings, occluders and weapons |

### R7 — Skeletal entities and optional acceleration

**Exit gate:** Individually tested optional capability sets: immutable bind geometry + per-entity poses; Jolt ragdolls and hit zones; mesh/task shaders, software micropolygon and RT acceleration backed by measured hardware gains.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R7-01` | [#67](https://github.com/SIGAAMDAD/SIREngine/issues/67) | Shared bind geometry, rig and GPU skinning |
| `VG-R7-02` | [#68](https://github.com/SIGAAMDAD/SIREngine/issues/68) | Dynamic character Jolt collision and ragdolls |
| `VG-R7-03` | [#69](https://github.com/SIGAAMDAD/SIREngine/issues/69) | Optional mesh shaders, software raster and RT |

### R8 — Editor, profiling, release and independent acceptance

**Exit gate:** editor/re-cook inspector, measured performance, 100% locally compilable first-party line/branch/function coverage, CTest/sanitizers, required hardware testing for each qualified tier, and zero source displacement texture in shipping packages. Optional capability QA and per-driver certification #273 are tracked independently.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R8-01` | [#70](https://github.com/SIGAAMDAD/SIREngine/issues/70) | Editor/diagnostic mesh residency inspector |
| `VG-R8-02` | [#71](https://github.com/SIGAAMDAD/SIREngine/issues/71) | Performance and memory benchmarking |
| `VG-R8-03` | [#72](https://github.com/SIGAAMDAD/SIREngine/issues/72) | Baseline integration and shipping acceptance |
| `VG-R8-04` | [#75](https://github.com/SIGAAMDAD/SIREngine/issues/75) | Optional skeletal/ragdoll capability QA |
| `VG-R8-05` | [#76](https://github.com/SIGAAMDAD/SIREngine/issues/76) | Optional mesh/task, software raster and hardware RT capability QA |
| `VG-R8-06` | [#273](https://github.com/SIGAAMDAD/SIREngine/issues/273) | Per-driver scene/cut/capability and real hardware acceptance |


## 5. Shared memory / object treatment

| Category | Canonical/shared memory | Per-instance / derivative memory | Owner(s) |
| --- | --- | --- | --- |
| Pebbles, foliage, sticks, clutter | One geometry/cluster set per prototype, versioned under GRDG | Compact spatial transforms and variation; selected Jolt bodies on promotion; GPU wind/output only when required | World/Clutter, GPUScene, Jolt |
| Rocks, boulders, modular buildings, occluders | Immutable CPU vertex/index streams; shared Jolt-compatible collision source and CPU BVH query references; one compressed GPU mesh page set per asset/variant | Transforms, collision policy, Jolt body registration, visibility/material selections, HLOD/terrain cell activation | GRDG/NMD, MeshCache, Jolt, World and GPUScene |
| Weapons | One mesh asset and collision-shape definition | Carried skeletal attachment or dynamic dropped body with same mesh identity | Animation, World, Jolt |
| NPCs / player / skeletal meshes | Immutable bind-pose mesh, skeleton hierarchy and animation clips | Per-entity bone pose, controller/ragdoll bodies, transient skin buffers, optional RT acceleration | Animation, Jolt, GPUScene and graphics driver |

The engine must account for **distinct physical allocations once**, even when the same resource is referenced by multiple providers or instances. CPU collision BVH nodes and GPU BLAS/TLAS are derived allocations with independent lifetimes; source vertex pointer sharing is literal only for compatible CPU query/physics representations, not a fictional universal zero-copy GPU/Jolt pointer.

## 6. Owners and merge boundaries

| Owner | Exclusive mutable authority | May consume | Must not introduce |
| --- | --- | --- | --- |
| GRDG resource owner | Canonical mesh keys, revision, publication, read/provider pin lifetime | Mesh cooker provenance and provider acknowledgments | Renderer-owned independent resource graph |
| SIRMemoryManager | Physical CPU backing allocation/accounting | Geometry source/collision needs | Hidden untracked heap path |
| MeshCache | CPU cache slots, logical geometry references, page metadata | GRDG resource read/provider pins | A second GRDG writer or GPU allocator |
| Geometry cooker/SIRPack | Cooked format, hashes, cluster geometry, offline displacement | Source editor images (cook-only) | Shipping displacement source texture dependencies |
| Renderer/GPUScene/FrameGraph | Instance and draw snapshots, shared cull/LOD, pass ordering | MeshCache page/geometry directories | Driver-specific independent scene graph |
| World/Streaming | Geometry and world cell demand, epoch/cancel policy | GPU request feedback and I/O | Mesh-residency-owned second demand scheduler or rejected provider adapter |
| Physics/Jolt | Shape/BVH, rigid-body state, collision policy | Pinned immutable CPU triangle views | Duplicating canonical vertex data per static instance |
| Each native graphics driver | Native buffers or client arrays, per-API draw/queue lowering and completion | Common cooked geometry/cut/draw plan and frozen graphics ABI v1 | Driver-specific LOD or streaming policy; fictitious unsupported compute/indirect features |
| Independent QA | Acceptance evidence and fault injection | Exact source SHA and hardware captures | Simulated or stale PASS as production verification |

Integration requires an agreed frozen cross-owner ABI and exact source ancestry; work may be developed independently inside protected paths until those gates are passed. Do not merge any still-diverging GRDG/Streaming/driver feature branch by assumption.

## 7. Implementation exit criteria

1. **Source and style:** SIREngine-owned C++17 follows pinned clang-format 23.1.5, MPLv2 header notice, global namespace, typedef struct/enum conventions, four-param API cap, mandatory method grouping, source-grounded AST checks.
2. **Local coverage:** 100% line, branch and function coverage for all locally compilable first-party code within the active implementation scope, measured from source-based coverage and exact build flags; also test negative states. Hardware/platform-only paths reported separately until executable.
3. **Memory / concurrency:** Immutable source bytes never reclaimed before last read/provider pin; GPU pages never recycled before graphics/compute/copy and AS consumers retire; no hidden SEALED allocations. Reused world objects do not duplicate canonical buffers.
4. **Visual:** Conservative visible set and root fallback, crack-free LOD/HLOD boundaries, shadow/material/depth parity, transparent/masked capability-scoped results and stable rapid camera turns.
5. **Physics:** Actual shared static CPU positions where selected, Jolt collision/reference parity and correct per-instance collision modes, ragdoll transitions separately scoped.
6. **Shipping:** Baked static displacement source textures absent from runtime package, graph closure and filesystem; loader, geometry renderer and collision work with original authoring heightmap deleted.
7. **Hardware / performance:** per-tier real GL1.2/1.5/2.x/3.3/4.6 and D3D7/8/9/10/11/12 plus Vulkan device proof where the named backend ships (not CPU mock alone); drivers and OS with no available hardware marked NOT_RUN; measured memory/CPU submit and qualified GPU timing, CMake/CTest/GoogleTest and sanitizer stress, exact source & driver IDs and p50/p95/p99 data. Track GPU geometry separately from textures, staging, descriptors and RT structures.
8. **Licensing:** Unreal Nanite source is behavior reference only; do not copy its implementation/shaders. Permissively licensed donors must be pinned and attributed.

## 8. Completion conventions

- Each issue opens with `Task ID`, source touchpoints, direct blockers, writer, implementation contract, acceptance, negative cases and protected areas.
- Mark a task completed only when the commit, compile/config/format/AST records and required test/benchmark evidence are attached to the exact issue. Passing a standalone CPU mock cannot certify hardware Vulkan/D3D12 behavior.
- R8-03 is the **initial modern GPU virtualized geometry** release gate (Vulkan/D3D12). R8-06 (#273) separately qualifies each other native graphics-driver tier and the shared CPU/reference core. R8-04 separately gates skinned/ragdoll capabilities; R8-05 independently gates mesh/task, software-raster and RT capabilities. Releases can distinguish supported hardware tiers without changing core mesh-format authority.
- Roadmap updates should edit this index and its corresponding GitHub issues together. Preserve direct issue dependency references.

**Tracking:** [Master GitHub issue #36](https://github.com/SIGAAMDAD/SIREngine/issues/36) · [Source foundation](https://github.com/SIGAAMDAD/SIREngine/blob/feature/mesh-cache-foundation/Docs/Architecture/20-mesh-cache-foundation.md)

## 9. Universal Nanite-derived execution contract (2026-11-04)

**Owner request:** shared virtualized-geometry system for *all* graphics drivers, not Vulkan/D3D12-specific scene or streaming systems. This section is the binding cross-tier update to the original R3/R4/R8 GPU-first plan. Implementation tasks: #268, #269, #270, #271, #272 and #273. Native graphics ABI remains v1.0 and public shared graphics interfaces are owned by ABI #130.

### 9.1 Invariants and pipeline

1. **One cooked geometry authority:** SIRPack owns one versioned, deterministic cluster-group hierarchy with per-group bounds, monotonic geometric error, parent/child coverage, seam compatibility and material boundaries. Root coarse data must be admitted and pinned for any asset accepted on a device. Original displacement textures are absent from the runtime package by existing R1 terms. No per-backend mesh cook or scene ID.
2. **One complete surface cut:** for every visible surface domain, either a parent coverage group or its *entire* child replacement group is selected, never both and never a partially resident group. Selection is by world/instance transform, frustum/LOD/error, optional occlusion and current complete-group residency, not by graphics API name. Required group pages (including seam/format derivatives) are published atomically and pinned until all commands using them finish.
3. **One shared scene/LOD/draw plan:** CPU-side immutable scene snapshot (SoA instance+mesh key+revision+transform+material), per-view projected screen error and bounds, group IDs, selected resident page IDs, pass/material sorting and draw-range records. The plan contains logical IDs and source/device generations, no persistent native `GLuint`, `ID3D*`, `VkBuffer` or driver pointers. A backend maps logical records to native resources.
4. **One page/demand owner:** GRDG canonical identity/revision; MeshCache immutable CPU read leases; Core Memory physical CPU backing; World/Streaming one request scheduler; SIRRenderingDevice native GPU/CPU-facing resources. CPU missing-page demand and optional bounded GPU feedback enter the same World/Streaming queue with epoch/cancel/source-revision validation. A stale request cannot activate old pages or evict the pinned root.
5. **No mandatory advanced features:** CPU-selected complete cuts + legal indexed draws define the universal executable baseline; compute/SSBO/indirect/mesh shader/Hi-Z/occlusion query are selected by actual native capability, never required for an older driver. Shared graphics ABI remains **v1.0** and all special draw commands are optional v1 capabilities owned by #130.
6. **Conservative visibility:** frustum + optional CPU BSP/PVS, cone and delayed occlusion may reject only proven invisible groups; unknown or stale occlusion keeps geometry visible. Reuse the shared Hi-Z #46 only on qualified GPU tiers. Depth, alpha and shadow passes may use independently budgeted cuts but must pin their own selected pages until completion.
7. **Capacity-bounded and recoverable:** static memory/page/command/work capacities selected before sealing; overflowing a queue or out-of-budget streaming preserves a valid coarser root and returns a typed budget/fallback result. CPU client arrays and legacy VBOs remain alive until real native submission completion; GPU page retirement waits for actual device completion and generation.

### 9.2 Portable data and callable seams (proposals, freeze with #38 and ABI #130)

- `sirVGClusterGroup_t`: stable group/parent relationship, mesh revision, geometry error, bounds/cone, surface coverage key, packed page dependencies, material/vertex layout IDs and required seam group IDs. Cooked hierarchy accepts multiple sibling pages but activation is group-atomic.
- `sirVGResidencySnapshot_t`: immutable per-device/page-generation complete-group availability and root pins, with CPU/GPU derivative readiness and submission completion; produced by the single residency owner.
- `sirVGSceneSnapshot_t`: immutable per-frame SoA instance list with canonical mesh IDs/revisions, transforms, material recipe identities and validated bounds; GPUScene table is an optional mirrored projection.
- `sirVGFramePlan_t`: canonical per-view ordered logical selected groups/draw ranges/material/pass memberships plus pending page demand. No native command handles.
- `sirVGExecutionCaps_t`: **internal** capability classification: client arrays / indexed vertex buffers / programmable state / compute + SSBO + indirect / mesh or task shaders; orthogonal flags for hardware occlusion and native fences; derives from actual shared graphics ABI v1 capabilities.
- `SIRVGTraversal::selectCut(view, scene, residency, plan)`: read-only selection producing complete groups or explicit bounded failure.
- `SIRVGDrawCompiler::compile(plan, caps, batches)`: material-sort and group-aggregate results into a bounded `sirVGExecutionPlan_t`. Drivers only lower typed batches using normal graphics commands. Any shared binary ABI addition belongs to #130; these example C++ calls are internal proposal contracts until compiled and frozen.

### 9.3 Native execution tiers

| Driver/API | Common cut execution | Native lowerer |
| --- | --- | --- |
| GL1.2 | CPU BVH/PVS/LOD, fixed shaderless | client array indexed draw, material/lightmap-ordered larger batches |
| GL1.5 | CPU BVH/LOD plus delayed optional occlusion query | core VBO indexed batches; optional ARB program recipe separately |
| GL2.0/2.1 | CPU LOD/visibility | VBO/IBO, GLSL1.10/1.20 or fixed state; extension FBO only where valid |
| GL3.3 | CPU LOD/visibility and instancing | VAO/VBO/IBO/UBO, indexed/instanced draw; no compute/indirect core |
| GL4.6 | qualified GPU cluster/Hi-Z/compaction, CPU reference fallback | compute + SSBO + multi-draw indirect, correct GL barriers; optional extension mesh shaders |
| D3D7/D3D8 fixed | CPU BVH/PVS/LOD | fixed-function VB/indexed draws, actual texture/light and index limits |
| D3D8 programmable / D3D9 | CPU LOD and batch sorting | bounded native VB/IB+shader variants; avoid assumptions about compute or indirect |
| D3D10/10.1 | CPU LOD/visibility | VB/IB indexed batches; no general DX12-style indirect execution |
| D3D11 FL9_x/10_x | CPU LOD/visibility | D3D11 indexed/instanced draws, feature-level shader/format limits |
| D3D11 FL11 | qualified compute/UAV/indirect if real caps permit | compute cull, indirect indexed draws or CPU batch fallback |
| D3D12 / Vulkan | GPU cull, indirect indexed, CPU reference validation | native compute/graphics and explicit memory/synchronization; mesh/task optional |
| Software provider (only when actually registered) | CPU cut | bounded CPU triangle raster or other defined software draw provider; no new backend registration implied |

**Batching:** no draw-per-small-cluster on fixed/programmable hardware. Aggregate contiguous resident indices and compatible material passes into bounded larger batches; split 16-bit index windows and maintain each driver’s baseVertex/firstInstance semantics (CPU rebase or typed rejection). Reduced variant materials are authored/cooked, not silently guessed. The visual output may use tier-specific shading; geometric coverage and cut error are portable.

### 9.4 Page lifecycle and exact failure behavior

`ROOT_READY -> REQUESTED -> LOADING -> VERIFIED -> STAGED -> INSTALLED -> GROUP_READY -> ACTIVE`; cancellation/failure can only return to a still-valid ancestor. `INSTALLED` is not visible until every sibling/dependency and native upload completion is confirmed. An immutable frame-plan pins its complete cut and any backend-native derivative through the last CPU/GL/D3D/Vulkan submission completion. Superseded pages are `RETIRED_PENDING` until all readers/views/queues quiesce; only then `RECLAIMED`. Destroy/recreate device increments generation; all derived handles and occlusion history invalidate; canonical SIRPack/GRDG source revisions remain unaffected. A streaming teleport can select coarser roots immediately while fine pages are fetched.

### 9.5 Release and falsification matrix

- **Source:** one cooked SIRPack hash per mesh, deterministic clusters, monotonic group error, 100% group surface coverage and zero gaps/double-coverage on 1,000+ camera positions, LOD thresholds, region seams, teleports and camera rotations.
- **Residency:** randomized child-page delays, corrupt hashes, canceled I/O, full GPU/CPU pool, concurrent 2–3 in-flight frames, missing seam neighbors, device loss, stale revision and forced root-only device all preserve legal rendered coverage, no pointer UAF or early fence retirement.
- **API:** explicitly test no compute/indirect on GL1.x/2.x/3.3/D3D7–10 and D3D11 downlevel; GL1.2 no VBO; GL1.5 VBO/query result latency; GL4.6 and D3D11 FL11 GPU path only on verified native feature support; Vulkan/D3D12 indirect hazards and cross-queue completion.
- **Visual:** identical selected geometry groups for same CPU oracle inputs regardless of API; conservative GPU visibility permitted to draw extras but may not discard a covered group; compare pixel silhouette and terrain/mesh seam geometry with material-specific tolerance, not byte-equal shading across eras.
- **Performance:** capture draw count per material, cluster/triangle counts, CPU cut and batch/sort costs, I/O/decode/upload, physical logical page residency, p50/p95/p99 frame and scene update, GPU timings only when hardware provides them. Compare against conventional mesh LOD at equal rendered error across a representative world route. Every OS/API/GPU test is PASS/FAIL/NOT_RUN separately, with source revision and device driver ID.
- **Quality:** C++17 pinned formatter, real Clang AST, CMake/CTest/GoogleTest, sanitizer and locally compiled source line/branch/function coverage; no QA pass from a mock or from issue text.

**Scope and progress:** Documentation/issue roadmap only. Source contracts, geometry cooker, native drivers and GPU tests are not completed by this update. Existing 29 tasks remain open; new cross-tier tasks #268–#273 are open.

# SIR-VG-001 — Shared Mesh Cache and Virtualized Geometry Roadmap

**Canonical tracking issue:** [#36](https://github.com/SIGAAMDAD/SIREngine/issues/36)
**Implementation branch:** [`feature/mesh-cache-foundation`](https://github.com/SIGAAMDAD/SIREngine/tree/feature/mesh-cache-foundation) at original foundation commit [`9f0a566a8d9d`](https://github.com/SIGAAMDAD/SIREngine/commit/9f0a566a8d9d091ebee47e4d701dbffe1a2b5b8e).
**Base branch:** `develop` at `51666fadadf8112b3b2d5bf919ba11dde55d931b`.
**Status (2026-11-04):** R0 independent CPU cache slice committed; 28 roadmap implementation/acceptance issues open. Remaining stages are requirements and executable plans, **not** implemented source.
**Source plan:** SIR-VG-001 (Nanite-inspired SIREngine-native virtualized geometry), SIR-VG-DISP-001 (baked displacement), existing `Docs/Architecture/20-mesh-cache-foundation.md`, and renderer/GRDG/Streaming design authorities.

## 1. Objectives and invariant boundaries

SIREngine will share immutable static source geometry across rendering, CPU physics/collision, BVH queries and many world instances. One canonical mesh identity/revision belongs to GRDG; CPU memory is physically allocated/accounted through SIRMemoryManager; `SIRMeshCache` owns logical geometry lookups/leases; `SIRRenderingDevice` alone owns GPU buffers and native queue submissions. Streaming demand is unified through World/Streaming, not another mesh-specific scheduler.

The independent CPU foundation in `Source/Mesh/SIRMeshCache.*` deliberately **does not depend** on unmerged GRDG, Jolt, Terrain3D or GPU drivers. Its published semantics currently cover caller-provided fixed-capacity slot backing, immutable borrowed source pointers, source-pin callback retirement, generation-safe handles and duplicate key rejection. It has native CMake and unit test targets. Earlier local validation reported 84.76% line and 71.63% branch coverage; **R0-01 is outstanding** and requires fresh exact-SHA results.

**Shipping displacement contract:** Static mesh height/displacement textures are authoring inputs only. Cook adaptive subdivisions and displaced final positions, optimize via meshoptimizer, produce compressed vertex/index/cluster/HLOD geometry and optional displaced collision, then ship *only* cooked data. A shipping SIRPack must contain no original displacement texture bytes, loadable source paths/IDs, runtime displacement sampler/binding, or transitive source dependency. Runtime uses precomputed LOD pages, without initial-scene or per-frame displacement/tessellation work for these baked assets.

**Backend contract:** Vulkan/D3D12 (and other backends that later qualify) lower the same mesh, visibility and residency results through native commands. Indexed-indirect-count is baseline; mesh/task shaders, compute micropolygon and RT are optional measured capabilities. Do not transplant Epic-licensed Nanite code. Pin permissive meshoptimizer donor versions and license notices.

## 2. Stage index

| Stage | Workstream | Issues | Direct links |
| --- | --- | ---: | --- |
| R0 | Independent baseline and authority freeze | 2 | [#37](https://github.com/SIGAAMDAD/SIREngine/issues/37), [#38](https://github.com/SIGAAMDAD/SIREngine/issues/38) |
| R1 | Offline cooking, compressed clusters, no-source-map shipping | 4 | [#39](https://github.com/SIGAAMDAD/SIREngine/issues/39), [#40](https://github.com/SIGAAMDAD/SIREngine/issues/40), [#41](https://github.com/SIGAAMDAD/SIREngine/issues/41), [#42](https://github.com/SIGAAMDAD/SIREngine/issues/42) |
| R2 | CPU canonical memory, Jolt shapes, BVH and collision policy | 3 | [#43](https://github.com/SIGAAMDAD/SIREngine/issues/43), [#44](https://github.com/SIGAAMDAD/SIREngine/issues/44), [#45](https://github.com/SIGAAMDAD/SIREngine/issues/45) |
| R3 | Sealed GPU geometry page residency and driver ABI | 3 | [#47](https://github.com/SIGAAMDAD/SIREngine/issues/47), [#48](https://github.com/SIGAAMDAD/SIREngine/issues/48), [#49](https://github.com/SIGAAMDAD/SIREngine/issues/49) |
| R4 | GPUScene, cluster traversal, first images | 3 | [#50](https://github.com/SIGAAMDAD/SIREngine/issues/50), [#51](https://github.com/SIGAAMDAD/SIREngine/issues/51), [#52](https://github.com/SIGAAMDAD/SIREngine/issues/52) |
| R5 | Streamed world geometry, terrain and HLOD integration | 3 | [#53](https://github.com/SIGAAMDAD/SIREngine/issues/53), [#54](https://github.com/SIGAAMDAD/SIREngine/issues/54), [#55](https://github.com/SIGAAMDAD/SIREngine/issues/55) |
| R6 | Material recipes, clutter and static object lifetimes | 3 | [#62](https://github.com/SIGAAMDAD/SIREngine/issues/62), [#64](https://github.com/SIGAAMDAD/SIREngine/issues/64), [#66](https://github.com/SIGAAMDAD/SIREngine/issues/66) |
| R7 | Skeletal entities and optional acceleration | 3 | [#67](https://github.com/SIGAAMDAD/SIREngine/issues/67), [#68](https://github.com/SIGAAMDAD/SIREngine/issues/68), [#69](https://github.com/SIGAAMDAD/SIREngine/issues/69) |
| R8 | Editor, profiling, release and independent acceptance | 4 | [#70](https://github.com/SIGAAMDAD/SIREngine/issues/70), [#71](https://github.com/SIGAAMDAD/SIREngine/issues/71), [#72](https://github.com/SIGAAMDAD/SIREngine/issues/72), [#75](https://github.com/SIGAAMDAD/SIREngine/issues/75) |

## 3. Dependency DAG

```text
Independent baseline commit 9f0a566
    |-- R0-01 coverage closure -------------------------------------------+
    |-- R0-02 shared contract freeze                                      |
                |                                                          |
                +--> R1-01 SIRPack --> R1-02 cluster hierarchy             |
                |                          |                               |
                |                          +--> R1-03 displacement         |
                |                                     |                    |
                |                                     +--> R1-04 shipping  |
                |                                                          |
                +--> R2-01 canonical GRDG/NMD leases <---------------------+
                |          +--> R2-02 Jolt shared static collision
                |                     +--> R2-03 CPU BVH and queries
                |
                +--> R3-01 sealed GPU pages + R3-02 indirect device ABI
                           +--> R3-03 page publication
                                +--> R4-01 GPUScene
                                +--> R4-02 cluster cut / Hi-Z
                                        +--> R4-03 Vulkan/D3D12 parity
                                        +--> R5-01 World/Streaming feedback
                                              +--> R5-02 World Partition
                                              +--> R5-03 Terrain HLOD
                                              +--> R6-01 materials/shadows
                                              +--> R6-02 clutter
                                              +--> R6-03 static world/weapon lifecycle
                                              +--> R8-01 editor / R8-02 perf
                                              +--> R8-03 baseline acceptance
                                              |
                                              +--> R7-01 skeletal and skinning
                                                   +--> R7-02 ragdoll physics
                                                   +--> R7-03 advanced GPU (independent)
                                                   +--> R8-04 optional capability QA
```

The diagram is conceptual; **each task issue's explicit `Blocked by` list is the binding issue-level dependency specification**. R7 capabilities and R8-04 do not block baseline R8-03. No calendar release dates are assigned without measured implementation throughput and remaining prerequisites.

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

**Exit gate:** Physical GPU memory allocated by driver within sealed budget; logical generation-safe page table and root fallback; native indexed-indirect-count and cross-queue fences; atomic page publication/retirement.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R3-01` | [#47](https://github.com/SIGAAMDAD/SIREngine/issues/47) | Fixed GPU geometry page pool and directory |
| `VG-R3-02` | [#48](https://github.com/SIGAAMDAD/SIREngine/issues/48) | Native Vulkan/D3D12 indexed-indirect-count ABI |
| `VG-R3-03` | [#49](https://github.com/SIGAAMDAD/SIREngine/issues/49) | Dependency-complete GPU page install and retirement |

### R4 — GPUScene, cluster traversal, first images

**Exit gate:** Packed instance records, bounded dirty uploads, CPU reference cluster cut, conservative frustum/cone/Hi-Z; real Vulkan and D3D12 screenshot/depth equivalence with stable root fallbacks.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R4-01` | [#50](https://github.com/SIGAAMDAD/SIREngine/issues/50) | GPUScene instance tables and dirty journals |
| `VG-R4-02` | [#51](https://github.com/SIGAAMDAD/SIREngine/issues/51) | Hierarchical cluster culling and screen-space LOD |
| `VG-R4-03` | [#52](https://github.com/SIGAAMDAD/SIREngine/issues/52) | Vulkan/D3D12 indexed-indirect first-image parity |

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

**Exit gate:** Editor/re-cook inspector, reproducible measured performance, 100% locally compilable first-party line/branch/function coverage, CTest/sanitizers, GPU hardware and zero source displacement texture in shipping packages; optional capability QA separate.

| ID | GitHub issue | Implementation slice |
| --- | --- | --- |
| `VG-R8-01` | [#70](https://github.com/SIGAAMDAD/SIREngine/issues/70) | Editor/diagnostic mesh residency inspector |
| `VG-R8-02` | [#71](https://github.com/SIGAAMDAD/SIREngine/issues/71) | Performance and memory benchmarking |
| `VG-R8-03` | [#72](https://github.com/SIGAAMDAD/SIREngine/issues/72) | Baseline integration and shipping acceptance |
| `VG-R8-04` | [#75](https://github.com/SIGAAMDAD/SIREngine/issues/75) | Optional skeletal/ragdoll/RT capability QA |


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
| Vulkan and D3D12 | Native buffers, descriptor heaps, queue fences, indirect command lowering | Frozen shared renderer ABI | Divergent LOD/streaming algorithms |
| Independent QA | Acceptance evidence and fault injection | Exact source SHA and hardware captures | Simulated or stale PASS as production verification |

Integration requires an agreed frozen cross-owner ABI and exact source ancestry; work may be developed independently inside protected paths until those gates are passed. Do not merge any still-diverging GRDG/Streaming/driver feature branch by assumption.

## 7. Implementation exit criteria

1. **Source and style:** SIREngine-owned C++17 follows pinned clang-format 23.1.5, MPLv2 header notice, global namespace, typedef struct/enum conventions, four-param API cap, mandatory method grouping, source-grounded AST checks.
2. **Local coverage:** 100% line, branch and function coverage for all locally compilable first-party code within the active implementation scope, measured from source-based coverage and exact build flags; also test negative states. Hardware/platform-only paths reported separately until executable.
3. **Memory / concurrency:** Immutable source bytes never reclaimed before last read/provider pin; GPU pages never recycled before graphics/compute/copy and AS consumers retire; no hidden SEALED allocations. Reused world objects do not duplicate canonical buffers.
4. **Visual:** Conservative visible set and root fallback, crack-free LOD/HLOD boundaries, shadow/material/depth parity, transparent/masked capability-scoped results and stable rapid camera turns.
5. **Physics:** Actual shared static CPU positions where selected, Jolt collision/reference parity and correct per-instance collision modes, ragdoll transitions separately scoped.
6. **Shipping:** Baked static displacement source textures absent from runtime package, graph closure and filesystem; loader, geometry renderer and collision work with original authoring heightmap deleted.
7. **Hardware / performance:** Vulkan and D3D12 GPU hardware captures (not CPU mock alone), driver memory counters, CMake/CTest/GoogleTest, sanitizer stress, exact source & driver IDs and p50/p95/p99 data. Track GPU geometry separately from textures, staging, descriptors and RT structures.
8. **Licensing:** Unreal Nanite source is behavior reference only; do not copy its implementation/shaders. Permissively licensed donors must be pinned and attributed.

## 8. Completion conventions

- Each issue opens with `Task ID`, source touchpoints, direct blockers, writer, implementation contract, acceptance, negative cases and protected areas.
- Mark a task completed only when the commit, compile/config/format/AST records and required test/benchmark evidence are attached to the exact issue. Passing a standalone CPU mock cannot certify hardware Vulkan/D3D12 behavior.
- R8-03 is the **baseline virtualized geometry** release gate. R8-04 separately gates optional skinned/ragdoll/mesh/task/software-raster/RT capabilities. Releases can distinguish supported hardware tiers without changing core mesh-format authority.
- Roadmap updates should edit this index and its corresponding GitHub issues together. Preserve direct issue dependency references.

**Tracking:** [Master GitHub issue [#36](https://github.com/SIGAAMDAD/SIREngine/issues/36)](https://github.com/SIGAAMDAD/SIREngine/issues/36) · [Source foundation](https://github.com/SIGAAMDAD/SIREngine/blob/feature/mesh-cache-foundation/Docs/Architecture/20-mesh-cache-foundation.md)

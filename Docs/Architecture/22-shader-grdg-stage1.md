# Stage 1 — Shader foundation against merged GRDG

**Implementation branch:** `feature/shader-pipeline-foundation` (draft PR #128).  
**Merge inputs:** shader `03a49fa5204aef7447fd7f00ff844339faf8d86d` and `develop` `1e799f4283c054b1a891ad23acc09c39d690b9c1`.  
**Scope:** reconcile source ancestry and native CMake registrations; retain shader package schema 1 and graphics ABI **v1.0**. This integration does not advance GPU/FrameGraph milestones.

## Canonical source ownership

- `Source/Resources` is the sole owner of GRDG resource identity, immutable publication, resource dependencies, revisions, read scopes, allocation accounting and provider pins.
- `Source/Renderer/Shaders` and `Source/Renderer/SIRShaderPipeline.*` own shader package validation, selection, portable identities and bounded warmup keys. They do not create a shadow resource graph or own native GPU objects.
- `Tools/ShaderCooker` owns offline source-DAG expansion, DXIL/SPIR-V compilation and stage package artifacts. It has no runtime GRDG publisher or renderer device.
- Core Jobs owns CPU worker scheduling; FrameGraph owns future per-frame GPU pass scheduling. Driver implementations exclusively own native shader modules, descriptor sets, PSOs and GPU queues.

## Exact GRDG-to-shader resource mapping for SHD-05

| Data | GRDG identity | Shader-side use |
| --- | --- | --- |
| Cooked shader package | `SIRResourceKey {assetId,typeId,variantId}`, semantic shader type, `SIR_RESOURCE_KIND_BLOB` | Package `nAssetId` and selected stage content; `typeId` is centrally registered, never assigned locally |
| Material definition | Typed `SIR_RESOURCE_KIND_COMPOSITION` | Borrowed `SIRMaterialView` plus renderer-owned material decoder and binding plan |
| Shader and image dependencies | `SIRResourceDependency` required/optional/streamed | Exactly selected recipe dependencies and invalidation sequence; missing optional variants do not become hard requirements |
| Live native shader/PSO | `SIRDerivedResourceKey` + provider pin and exact GRDG resource revision | Driver-native generation, full PSO identity and deferred release |
| Cooker output | Staging followed by `SIRResourceGraphWriter::publishExpected` | Reject obsolete asynchronous publications without mutating the current canonical revision |

**Revision semantics:** `sirShaderPackageHeader_t::nSourceRevision` is the cooked source revision; `SIRResourceInfo::m_nRevision` is the GRDG publication revision. They are different fields and must not be equated without an explicit cooker-to-publication mapping. A material cache must additionally account for selected dependent resource revisions and the transitive invalidation sequence, including journal overflow/rebuild behavior.

**Borrowing:** `SIRResourceView` and its payload pointers are valid only inside an active `SIRResourceReadScope`. Any asynchronous job or driver completion retains its own exact provider pin or copies validated small metadata. No borrowed payload pointer survives the read scope.

**GPU retirement gap:** `SIRResourceDerivedRegistry::retireRepresentation` and `updateCompletedFence` currently use one monotonically increasing provider scalar, whereas `sirGraphicsCompletionSet_t` tracks separate graphics/compute/copy queue values and device generation. SHD-05/06 must introduce a queue-completion-aware provider bridge before claiming native PSO retirement safety. A raw queue fence cannot be substituted as the aggregate scalar.

## Graphics ABI v1 model boundary

Shared ABI issue #130 and branch `renderer/shared-driver-api-v1` own the canonical v1 pipeline model and capability types. Shader work consumes their completed integration later; it must not define parallel public graphics enums or increment graphics ABI version. Target models:

1. **MODERN:** offline specialized SPIR-V/DXIL/GLSL and optional GPU-driven passes where the device supports them.
2. **PROGRAMMABLE:** offline bounded shader profiles and CPU/stateful render-path lowering for D3D8–11 and GL2.x–3.x.
3. **FIXED_FUNCTION:** shaderless cooked state recipe, no fabricated shader module or compute capability, for supported older drivers.

The existing SHD-01 CLI currently *implements* DXC-based Vulkan/D3D12 SM6.6 targets only. Other targets remain explicit UNSUPPORTED until compatible cooker emitters and hardware tests exist; mapping an API enum to an output format alone is insufficient.

## Stage 1 native registrations

- Root `CMakeLists.txt` registers the offline `Tools/ShaderCooker` subtree once.
- `Source/Renderer/CMakeLists.txt` appends shader pipeline/contracts to the existing `SIRGraphics` target without replacing shared ABI, GRDG or native-driver registrations.
- `Tests/Unit/CMakeLists.txt` appends the two shader unit executables alongside all existing GRDG/Core/graphics tests.
- Native shader and cooker tests remain separate from hardware acceptance. SHD-00/01 closure retains their existing source-coverage/AST/full-build gates.

## Next work, outside this integration commit

Implement typed cooked shader/material format handling and revision-safe `SIRShaderLibrary` projections (SHD-02/05), the native multi-queue provider retirement bridge (SHD-05/06, shared ownership review), and the three cooked pipeline-era recipe target emitters (ABI-PM02/SHD-11) after ABI-PM00/01 is integrated. GPU FrameGraph, Hi-Z and virtual geometry remain separate owners.

## Verification status

This file records interface ownership and merge intent. No claim of native Windows/Linux build, 100% coverage, Vulkan/D3D hardware execution, or final SHD-00/01 acceptance is implied by documentation or by the Git-object merge itself.

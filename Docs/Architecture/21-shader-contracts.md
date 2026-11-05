# SHD-00 — Shader ABI, package and GPU-work contracts

**Issue:** [SHD-00 #78](https://github.com/SIGAAMDAD/SIREngine/issues/78). **Owner:** shared renderer ABI integrator. **Implementation branch:** `feature/shader-pipeline-foundation`.

## Source authority and boundaries

The authoritative graphics device contract remains `SIRRenderingDevice` and `sirGraphicsResult_t` in `Source/Renderer/SIRRenderingDevice.h` and `SIRGraphicsTypes.h`. `Shaders/SIRShaderContracts.h` is additive; it neither introduces a second device nor changes physical GPU ownership. `SIRShaderPipeline.h` retains its initial shader variant, cache-index and bounded warmup abstractions. SHD-00 provides the missing common *data contracts* consumed by ShaderCooker, shader library, material resolver, GPU work classification and native graphics drivers.

| Contract | Source owner | Consumers / lifetime |
| --- | --- | --- |
| Cooked binary, stage records, reflection, revision/target fingerprint | Offline ShaderCooker | Borrowed immutable bytes; GRDG publishes canonical asset identity, cooker validates before publication |
| `sirShaderMaterialPassSignature_t`, `sirShaderPipelineRecipe_t`, portable key | ShaderCooker and renderer material compiler | Exact structural equality on cache hits; no native GPU state |
| `sirShaderNativeIdentity_t`, native key | Backend-specific native shader/PSO cache | Native driver owns PSOs, device generation and opaque allocation; invalidate on device/driver/compiler/cache UUID change |
| Packed visible surface ID | Existing GPUScene/Hi-Z owner | Frame-local opaque 64-bit value; no canonical GRDG allocation or mesh lifetime encoded |
| Geometry lease | Mesh/provider owner | Borrowed canonical asset ID + source revision + provider epoch, with existing multi-queue completion set for retirement |
| Work admission and shading-rate eligibility | Renderer GPU work classifier | Pre-admitted capacities, no implicit allocation; FrameGraph alone executes actual passes |

## Versioned wire ABI (`SSHP` schema 1)

All integers are explicitly serialized **little-endian** with fixed offsets. C++ POD fields are **never serialized by copying their in-memory struct bytes**, and the serialized record sizes are independent of host padding. No native pointers, allocator handles, C++ enums beyond their fixed encoded widths, driver handles or GRDG objects are persisted. The reader rejects unsupported versions, malformed offsets, unsupported formats/capabilities, unsorted/duplicated binding coordinates, malformed reflection and mismatched binary/manifest fingerprints.

### Header: 64 bytes

| Byte offset | Wire type | Field |
| --- | --- | --- |
| 0 | u32 | magic `0x50485353` (`SSHP` in little-endian) |
| 4 | u16 | schema `1` |
| 6 | u16 | header size `64` |
| 8 | u32 | full package byte count, capped at 64 MiB |
| 12 | u16 | stage count, `1..32` |
| 14 | u16 | reflection binding count, `0..1024` |
| 16 | u32 | stage-record offset: `64` |
| 20 | u32 | binding-record offset: `64 + stageCount * 48` |
| 24 | u32 | binary payload offset: `bindingOffset + bindingCount * 24` |
| 28 | u32 | payload length, at least four bytes |
| 32 | u64 | nonzero canonical asset ID |
| 40 | u64 | nonzero source revision |
| 48 | u64 | nonzero manifest fingerprint |
| 56 | u64 | nonzero toolchain/target fingerprint |

The payload ends exactly at `nTotalBytes`; trailing data and overlapping sections are rejected. The manifest fingerprint is FNV-1a 64 over explicit ordered scalar inputs `(schema, assetId, sourceRevision, targetFingerprint, stageCount, bindingCount)` followed by the canonical stage and reflection record bytes. Every stage record separately checks its bytecode payload fingerprint using FNV-1a 64. **These checksums detect accidental corruption and stale data; they are not cryptographic signatures or a substitute for validating untrusted compiler binaries.**

### Stage: 48 serialized bytes

| Byte offset | Wire type | Field |
| --- | --- | --- |
| 0 | u32 | one single-bit `SIR_GRAPHICS_STAGE_*` mask |
| 4 | u16 | `sirShaderPass_t` (color/depth/shadow/velocity/gbuffer/compute) |
| 6 | u16 | `sirGraphicsShaderFormat_t` (DXIL/SPIR-V/GLSL/DXBC) |
| 8 | u64 | required renderer capability bits |
| 16 | u64 | binary content checksum |
| 24 | u64 | entrypoint signature checksum |
| 32 | u32 | bytecode payload absolute offset |
| 36 | u32 | bytecode payload length |
| 40 | u64 | binding signature checksum |

A compute pass requires a compute-stage record; a noncompute pass cannot use a compute-stage record. The stage must fit entirely inside the payload and have the required native capability tier. Its binding signature covers the entire canonical reflection table, not memory addresses.

### Reflection binding: 24 serialized bytes

| Byte offset | Wire type | Field |
| --- | --- | --- |
| 0 | u16 | binding set |
| 2 | u16 | binding index |
| 4 | u32 | existing `sirGraphicsBindingType_t` |
| 8 | u8 | frequency (frame/view/material/draw) |
| 9 | u8 | stage visibility mask |
| 10 | u16 | binding space |
| 12 | u16 | array length (nonzero) |
| 14 | u16 | reserved flags (must equal zero under schema 1) |
| 16 | u64 | nonzero canonical parameter name hash |

Bindings are strictly sorted by `(set, space, binding)`; duplicate coordinates are invalid even if one descriptor has a different type/frequency/stage mask. Shader stages referenced by bindings must exist in the package. Cooker-generated signatures and native root-signature/descriptor lowering must match this fixed typed reflection, not infer layout from arbitrary shader text at runtime.

## Pipeline identity and fallback ABI

` sirShaderPipelineRecipe_t ` includes stage/pass signature, color/depth format identity, MSAA sample count, alpha-mask, double-sided and velocity behavior, binding signature, source asset ID/revision, package schema and manifest, permutation, render state and required features. `sirShaderBuildPortableKey` returns a deterministic two-lane 128-bit *index*. Cache consumers must use `sirShaderRecipesEqual` on candidate hits; hashes are not unique IDs. A native key additionally incorporates backend, PCI/vendor/device, driver version, compiler fingerprint, cache UUID and device generation. Drivers must compare `sirShaderNativeIdentitiesEqual` on candidate native hits; a device generation change invalidates all live native PSOs. Cache binary validation and production eviction are subsequent SHD-06/09/10 work, not implied by the key API.

The portable key includes **all** cooked-package identity fields, including `nPackageManifestHash` and `nPackageSchema`. Two binaries with equal asset ID and source revision but different validated package manifests must produce different cache indices. Package validation also rejects duplicate `(pass, shader stage)` pairs, so runtime stage selection cannot depend on record order.

` sirShaderPassSignaturesEqual ` is the declared compatibility predicate: alpha-masked and opaque, shadow and color, velocity and color, distinct bindings and distinct sample configurations cannot silently share an uber pipeline. ShaderCooker SHD-07 may introduce explicit compatibility families only after the pass contract is validated and the same pass signature is honored.

## Visibility, generated geometry and budget ABI

- `sirShaderVisibilityId_t` packs into one u64: instance `[0..23]`, primitive `[24..43]`, material `[44..55]`, flags `[56..63]`. All-ones is the **invalid/no-hit** sentinel. The mesh source revision is kept separately in the generated-geometry lease/provider epoch, never accidentally truncated into visibility bits.
- `sirShaderGeometryLease_t` holds nonzero canonical asset ID, source revision, provider epoch and existing `sirGraphicsCompletionSet_t`. A lease only authorizes reuse when asset, revision, provider and device generations match **and the existing completion set covers every requested queue fence**. It remains a borrowed record; the producer retains source and native backing until all required queue completions retire.
- `sirShaderWorkBudget_t` is supplied by engine/game profile, not hardcoded for The Nomad. `sirShaderAdmitWork` rejects counts or scratch bytes exceeding caller-supplied visible-surface, material-bin, indirect-command, optional-dispatch or scratch capacities using `SIR_GRAPHICS_CAPACITY_EXCEEDED`. A caller must use an approved correctness-preserving fallback when capacity is exceeded; no silent work dropping.
- `sirShaderResolveRate` permits half/quarter rates only when derivatives, history and motion are valid and the surface is neither discontinuous nor alpha-masked. Otherwise it forces full rate. This contract is CPU policy only; GPU rate shading and numerical parity are SHD-15 scope.

## Integration and rejection rules

- **Error model:** use existing `SIR_GRAPHICS_SUCCESS`, `SIR_GRAPHICS_INVALID_ARGUMENT`, `SIR_GRAPHICS_UNSUPPORTED` and `SIR_GRAPHICS_CAPACITY_EXCEEDED`, never a parallel result enum.
- **Serialization:** ShaderCooker must write canonical little-endian records; runtime must check package size, schema, fingerprints, features, typed reflection and payload before passing a borrowed code view to the existing graphics backend.
- **GRDG:** owns canonical shader/mesh IDs and immutable revisions. SHD-00 code does not create new GRDG publishers or retain raw resource pointers.
- **Native backend:** owns physical PSOs/descriptors, device generation and queue fence retirement. No native API headers or hidden compiler dependencies enter the shared contract.
- **FrameGraph:** sole GPU submission/barrier/pass scheduler; work budget descriptors do not schedule actual GPU commands.
- **Cross-roadmap:** `#57` (FrameGraph) and `#38` (mesh/provider) retain their independent source owners. SHD-00 freezes consumer-facing value shapes; no source files from those workgroups are modified.

## Exact-source test contract

CTest: `renderer.shader.contracts` (standalone no-GPU C++17 test). The test executable covers each serialized record, eight stage feature tiers, schema/offset/digest corruption, reordered/duplicated typed reflection, unsupported formats, stale revisions, wrong device/queue generations, hash/identity matrix, packed-ID overflow/sentinel, bounded work overflow and rate fallback. CMake registers a distinct test target. CPU coverage may be measured with `gcov -b -c` or LLVM instrumentation against the exact compiler inputs; runtime hardware validation remains in backend/SHD-22 milestones. Passing a header-only or stub compiler test is not proof of a full SIREngine build.

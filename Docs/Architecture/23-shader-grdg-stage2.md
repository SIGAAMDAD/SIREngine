# Stage 2 — Typed shader/material GRDG resources

**Source branch:** `feature/shader-pipeline-foundation` (draft PR #128). Source-grounded against `develop` with GRDG merged.

## Cooked artifact schemas

- `SIRShaderMaterial` encodes a material manifest with a 48-byte explicit little-endian header and 32-byte pass records. The in-memory C++ header is independently aligned (56 bytes). This **does not change graphics ABI v1**.
- A manifest contains a stable asset ID, independent cooked source revision, pass count, dependency count, manifest FNV64 checksum, unique pass IDs, static permutation and render-state hashes, and shader dependency indices.
- `SIR_SHADER_MATERIAL_NO_SHADER` marks an explicitly fixed-function pass. Such passes cannot declare a compute pass and must advertise `SIR_GRAPHICS_FEATURE_FIXED_FUNCTION`. The material record contains authored render-state hash, so it is real canonical data rather than invented native allocation.
- `SIRShaderGRDG` reads existing SHD-00 shader packages and material records from GRDG's exactly-one canonical chunk (ID 0), validating both the canonical resource semantic family/format and the inner cooked format.
- Engine-assigned registered shader/material type IDs and format IDs enter `sirShaderGRDGFormats_t`; the shader subsystem does not allocate or override central type IDs.

## Ownership and invariants

1. **Single-writer publication.** The shader adapter stages data using `SIRResourceGraphWriter::allocateStaging`, copies validated cooked bytes once, and invokes `publishExpected` with the caller's exact expected GRDG revision. A failed publication cancels staging. GRDG remains the only canonical resource owner.
2. **Content-addressed backing.** Stage publication derives a deterministic two-part hash of the entire serialized package plus length and its semantic format ID. GRDG's native shared chunk table verifies full bytes on reuse, so identical bytes deduplicate; a hash match alone cannot alias different payloads.
3. **Scoped borrowing.** `sirShaderPackageProjection_t` and `sirShaderMaterialProjection_t` contain nonowning `SIRResourceView` and byte pointers that must not outlive the supplying `SIRResourceReadScope`. Their `nGraphRevision` is independent of the cooked header's `nSourceRevision`.
4. **Invalidation.** `sirShaderSnapshotMaterial` snapshots every direct dependency, including absent optional/streamed targets, and fingerprints stable keys, revision and residency. It does not allocate. The single writer can additionally apply `sirShaderStampMaterialInvalidation` to mix GRDG's *transitive* invalidation sequence, even when the material's own revision is unchanged. The caller regenerates the fingerprint to detect invalidation, and rebuilds after journal overflow.
5. **Native ownership.** The adapter creates no native driver handle, shader module, pipeline state object or provider registry. GPU completion remains queue-specific. Multi-queue completion aggregation and derived-registry retirement is a later dedicated integration with the shared ABI owner.

## Test acceptance

`renderer.shader.grdg` verifies cooked byte round-trips and checksums, optional/missing resources, shader/material typed projection, stage selection, GRDG byte deduplication, expected-revision rejection, immutable old-scope reads, dependency revision changes, propagated invalidation and writer memory reclamation.

Native clang-format, Windows/Linux builds, CTest and sanitizer/coverage results are tracked by CI against the exact committed SHA. Hardware-backed native PSO compilation and cross-compiler shader emission belong to subsequent milestones.

## Stage 2 compiler and archive additions

- `Tools/ShaderCooker/MaterialIR/SIRMaterialIR.*` validates a bounded typed, topologically ordered graph and rejects malformed operands. Static switch dependency masks, required/excluded combinations and capability requirements produce deterministic per-pass shader map entries. Dynamic switches do not multiply compile-time permutations; unused static bits share their stage hash. `shader.cooker.material-ir` tests these contracts.
- `Tools/ShaderCooker/Archive/SIRShaderArchive.*` implements archive schema **1** with a 64-byte little-endian header, 32-byte logical shader-stage reference entries, 24-byte deduplicated physical blob records and a bounded payload area. The archive is explicitly targeted to shared graphics ABI **v1**, with no ABI increment. Matching physical bytes are shared only after full memcmp, including across different logical entry-point/stage references. Full-byte manifest and per-blob checksums, strict indices, offsets, null/size and format validation are required before resolving any borrowed stage bytes.
- `Source/Renderer/Shaders/SIRShaderLibrary.*` resolves an authored material pass to a validated required shader dependency and one borrowed, stage-matched GRDG blob; it exposes both the cooked source revision and the canonical GRDG publication revision. Shaderless fixed-function passes return a state-only result. No duplicate GRDG resource manager, native shader module, PSO or resource-owned heap is created.
- `shader.cooker.archive` tests bytewise dedup, deterministic output, boundary and corruption rejection. The shader validation workflow must build and run **all** `shader.cooker.*` CTests, including material IR and archive.
- A native GPU driver still owns pipeline construction and multi-queue completion safety; source-only tests do not claim GPU execution. The `SIRPack` format owner is not yet implemented in the merged source and will provide later streaming I/O. This archive API operates on caller-owned validated buffers and does not fabricate SIRPack integration.

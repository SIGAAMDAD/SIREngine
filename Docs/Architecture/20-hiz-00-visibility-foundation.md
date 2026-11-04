# HIZ-00 — Visibility Foundation Contracts

## Status

HIZ-00 provides a compiled, backend-neutral CPU reference for reverse-Z hierarchical depth reduction and conservative rectangle culling. Native GPU shaders, indirect command execution, FrameGraph passes and rendering-method compilation are HIZ-02–HIZ-07 work; HIZ-00 does not claim they are implemented.

## Ownership and placement

- `Source/Renderer/Visibility/SIRVisibilityFoundation.h`: common rendering-method mode, depth and camera-history contracts; deterministic CPU visibility reference; explicit caller-owned buffers.
- `SIR::Graphics`: builds the reference regardless of native driver selection. No second GPU allocation owner.
- `SIRFrameGraph`, when integrated, owns per-frame read/write edges, queue transitions, history import/export and transient aliases. GRDG owns persistent resource revisions and provider backing.
- The graphics driver lowers native graphics/compute/indirect operations; it does not own culling policy.

## Input contracts

`SIR_VISIBILITY_AUTO` must resolve to a concrete policy before validation. GPU Hi-Z policies require compute, shader storage and indirect drawing capability. This HIZ-00 reference accepts reverse-Z only. The depth producer format is D16, D24S8 or D32F; sampling/resolve support must be independently negotiated on native hardware.

A view-history reuse decision requires matching stable view identity, projection and world-origin revision, depth-producer revision, scene-reset revision, exact render extent/sample count/depth format and device generation, with consecutive frame indices and no camera cut. This records history eligibility only; it does not establish GPU completion or depth-resource residency.

The CPU pyramid stores all mip levels contiguously, including full-resolution mip 0, using ceil-divide-by-two extents. Each destination pixel is the minimum of every existing source child, including partial 2x2 edge tiles. Input depth samples must be finite in [0,1]. Build operations validate all input samples before writing any output and require **non-overlapping caller-owned source and destination storage**. The caller must supply correct buffer capacities; no internal allocations occur.

`sirHiZTestRectangle` consumes half-open **mip-0 pixel rectangle coordinates** and a reverse-Z candidate nearest depth. It selects a mip so that the covered rectangle spans at most 2x2 pyramid texels. It examines **every covered texel**; a candidate is occluded only if `nearestDepth + bias < minDepth` for all of them. Off-viewport bounds and invalid pyramid sample values fail open as visible.

`sirVisibilityCullReference` performs two classification passes. It validates candidates and counts both outputs before writing IDs, so capacity and input failures cannot partially publish visibility results. The caller must keep inputs immutable for the call and keep output buffers disjoint from the inputs and each other. A false `bHistoryValid` conservatively returns every candidate visible. Rejected IDs can be consumed by a future two-phase recovery implementation.

## Verification

`Tests/Unit/SIRVisibilityFoundationTests.cpp` covers API validation, history invalidation, odd mip extents, depth min-reduction, buffer boundaries, null pointers, malformed layouts, capacity rejection, visible/rejected compaction and deterministic randomized differential tests. The relevant CTest name is `renderer.visibility.foundation`.

The reference `.cpp` is expected to reach 100% line/function/branch coverage under local LLVM source instrumentation. That metric applies only to this implementation file, not the GPU shaders, backend drivers or the full SIREngine repository. Native hardware execution and the subsequent FrameGraph interfaces require independent acceptance.

## HIZ-01/02 handoff

1. Rendering-method compiler binds a selected mode and authoritative view/depth producer contract.
2. FrameGraph imports the depth producer and explicit, completion-protected prior-frame Hi-Z backing; it creates transient compaction outputs and declares the hazards.
3. The shader cooker emits native kernels matching these reference semantics; differential GPU readback tests compare all pyramid mips and visibility outputs.
4. GPU indirect-count execution uses one shared ABI extension, then backend-specific Vulkan/D3D12/GL implementations; this HIZ-00 patch does not modify virtual methods in `SIRGraphicsCommandList`.

## HIZ-00 v1 ABI freeze and aliasing contract

The revision-1 CPU reference POD layout is compile-time pinned for mode/depth
enums, the method descriptor (16 bytes), mip descriptor (16 bytes), fixed
32-mip layout (536 bytes), and rectangle query (32 bytes). On 64-bit targets,
the view (72 bytes), candidate (40 bytes), cull descriptor (40 bytes), and
output descriptor (32 bytes) are pinned. Any future change to these public
layouts requires an explicit ABI revision and consumer migration.

The view revision counters are **exact-match equality tokens**, not
wall-clock timestamps. A view/frame gap, a camera cut, projection change,
origin change, producer/scene revision change, device-generation change,
size/sample change, or depth-format change invalidates reuse. Resource
residency and queue completion remain external prerequisites.

The pyramid builder rejects all overlapping addressed depth-input/output
byte ranges (including offset aliases). Culling rejects overlaps among the
**actually written** visible/rejected ID prefixes, caller-owned immutable
layout/candidate/pyramid inputs, the descriptor itself and its output
descriptor. Validation and both visibility-count passes finish before any
result is published; alias violations return
`SIR_GRAPHICS_INVALID_ARGUMENT` without writing result buffers or counts.
Zero-length ID prefixes need no backing storage. Output capacity exhaustion
continues to return `SIR_GRAPHICS_CAPACITY_EXCEEDED`.

The native CTest foundation target includes regression cases for offset
aliases in both directions, candidate/pyramid ID corruption, ID-array overlap
and output-descriptor overlap. The same CPU reference also provides the
semantic oracle for later HIZ-03/HIZ-05 GPU readback tests.

## GoogleTest and source-bound verification expansion

`SIRVisibilityFoundationGoogleTests.cpp` adds twelve separately discoverable
GoogleTest cases to the existing no-driver CTest oracle. The tests independently
check NPOT minimum reduction, reverse-Z strict comparison, invalid depth and
pyramid samples, source/output overlap, immutable input aliasing, transactional
capacity errors, conservative missing-history behavior, revision invalidation,
capability requirements, and randomized false-occlusion rejection.

The `Verification/Systems/HiZ00.json` manifest names the exact CPU source
root, two native CMake test targets, required CTest integration contracts and
100% line/branch/function/region thresholds. Its ASan and UBSan requirements
are independently exercised by the HIZ-00 CI sanitizer matrix. Successful
tests do not imply that the coverage threshold was reached; instrumented
LLVM profiles and exact-source coverage reports are still necessary.

## HIZ-00 source-bound coverage closure

LLVM 23 instrumented verification covers the two native visibility test targets and
measures `SIRVisibilityFoundation.cpp` independently for line, branch, function
and region coverage. Dedicated regression cases exercise byte-size overflow
rejection using caller-owned sentinel bytes without allocating enormous images,
and valid `uint32_t` output pointers that alias fields of the method descriptor
or layout. Output ID spans remain subject to explicit overlap validation.

The output descriptor and other live, non-nested C++17 typed descriptor objects
have disjoint object lifetimes by API contract; their addresses do not require
additional runtime byte-range checks against one another. Only on 32-bit targets
does `nCandidateCount` require a `size_t` multiplication overflow guard: its
32-bit count multiplied by the candidate POD size fits unconditionally on
64-bit targets. The guards continue to fail before any data access or output
publication for valid hostile capacities.

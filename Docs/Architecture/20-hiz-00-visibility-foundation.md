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

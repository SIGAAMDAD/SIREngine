# SIREngine graphics driver contract — version 1

**Status:** source-backed shared ABI foundation; renderer-neutral native test PASS. Native D3D12/Vulkan/OpenGL implementations must adopt this interface directly. No GRDG dependency and no compatibility adapters to earlier driver-specific frontends.

## Ownership and hierarchy

- `Source/Renderer/SIRGraphicsTypes.h`: portable backend IDs, capability bits, descriptors, 16-byte handles, queue-completion tokens, resource states, shader package formats and platform window tokens.
- `Source/Renderer/SIRRenderingDevice.h`: the **single contract** for `SIRGraphicsDriver` (adapter/device factory), `SIRRenderingDevice` (resource ownership, transfer, queue, presentation) and `SIRGraphicsCommandList` (recorded commands). Each native backend implements it; no D3D12/Vulkan/GL types appear in shared headers.
- `Source/Renderer/SIRGraphicsRegistry.{h,cpp}`: stack-resident, nonowning driver discovery and explicit API selection. One owner of every registered driver, no singleton, no heap allocations or per-frame registration.
- `Source/Renderer/SIRGraphicsValidation.{h,cpp}`: portable negative-argument and generation checks. Native implementations additionally enforce their capability/format/heap restrictions.
- `SIR::Graphics`: always-built C++17 shared target. Native drivers are opt-in children; the launcher links this target even without a native GPU driver.

## Selection model

`SIRGraphicsRegistry::registerDriver` is performed during platform/backend startup, before threads begin using rendering. An explicit backend choice selects **only that backend**; it fails `SIR_GRAPHICS_UNSUPPORTED` when no matching adapter supports the requested minimum API version, capabilities or software-adapter policy. There is no hidden cross-API fallback. `SIR_GRAPHICS_BACKEND_AUTO` selects the first registered driver with a qualifying adapter; registration order is therefore a platform configuration decision, not an implicit vendor preference.

The portable pipeline descriptor fixes the numerical meanings of topology, cull mode, depth comparisons, blend operations/factors, sampler filtering/addressing and shader-stage masks so every native backend lowers the **same** state values.

The version for `SIR_GRAPHICS_BACKEND_OPENGL` negotiates 2.1, 3.3, 4.6 etc. D3D7, 8, 9, 10, 11, 12 are explicitly distinguishable backends. `sirGraphicsCapabilities_t` advertises optional compute, multiple queues, storage, fixed-function, sampler feedback, mesh/ray, HDR and presentation. Drivers must reject unsupported operations explicitly. A legacy API reports fewer features; it does not change the common interface or require a compatibility renderer wrapper.

## Resource and memory invariants

- `sirGraphicsHandle_t` is 16 bytes: slot, resource generation, device generation and type. Zero generation is invalid. Slots may start at zero. Each driver maintains its own typed slot table, increases generation on reuse, and never revives a stale handle; generation exhaustion requires retiring the slot. No native pointer, RID, or GRDG identity leaks into the handle.
- `SIRRenderingDevice` is the sole native owner of GPU allocations and descriptor heaps. Handles are **borrowed references**, not separate allocation owners. A `retireResource` call invalidates the handle immediately, then releases physical backing only after **all** queue fences specified in its completion set are proven complete.
- Physical GPU memory accounting counts native heap backing once even when multiple views or aliases refer to the same bytes. `sirGraphicsMemoryBudget_t` separates committed/resident/mapped values. Native allocators are selected by the backend (D3D12MA, VMA, GL-owned memory), without leaking them to this API. Core/NMD memory owns CPU-side table, queue and descriptor metadata in `SIR_MEMORY_DOMAIN_RENDERING_CPU`.
- All buffer/texture sizes and capacity errors are explicit. Shipping frame hot paths must use preallocated arenas, pooled command objects and resident descriptor ranges; editor growth is a separately configured cold path. `SIR_GRAPHICS_CAPACITY_EXCEEDED` is a legitimate runtime result; no implicit heap growth or hidden CPU/GPU waits.
- `mapBuffer`, `flushBuffer` and `invalidateBuffer` have **explicit ranges** and must only succeed for supported host-visible memory; backends cannot return a pointer to unmappable GPU-local backing. Persistently mapped heaps remain owned by the backend.

## Recording, barrier and synchronization invariants

- A command-list is borrowed from a driver-owned pool, associated with one queue, and reset only after the prior submission on that pool slot has retired. `submit` takes a bounded array of closed lists and zero or more explicit GPU queue waits; success returns a generation-scoped completion token. `poll` and `wait` validate queue and generation. No hidden CPU wait in `submit`.
- `sirGraphicsCompletionSet_t` has one optional fence value per DIRECT/COMPUTE/COPY queue. Single-queue OpenGL and earlier APIs advertise only DIRECT and map GPU submission to their serial execution model. Backends validate the actual supported queue mask; they cannot fabricate asynchronous parallelism.
- `barrier` records transition, UAV ordering, or aliasing semantics. D3D12 Enhanced Barriers and Vulkan sync2 use their native capabilities. Drivers that cannot perform a requested transition/alias operation return `UNSUPPORTED`. The higher-level renderer owns barrier planning; GRDG is not required for the driver's local state validation.
- `sirGraphicsPipelineDesc_t` describes graphics, compute, or fixed-function pipelines. One offline cooked shader source produces backend-specific payloads (DXIL/SPIR-V/GLSL/DXBC). The backend validates shader format against its native API and uses binding descriptors to produce root signatures/descriptor sets or GL bindings. The API does not runtime-compile HLSL.
- The D3D7/D3D8 fixed-function path is explicitly selected by `SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION`, not a synthetic shader fallback in the driver. More specialized feature passes are negotiated through `nFeatureMask`.

## Native platform presentation

`createSurface` and `resizeSurface` accept an opaque `sirGraphicsNativeSurface_t` from the existing platform owner. `acquireSurfaceImage` returns a one-use surface/image acquisition token; an explicit queue submission consumes it and `present` matches the acquired image/window generation, allowing Vulkan acquire semaphores or equivalent native synchronization to remain backend-private. Only the platform code manages HWND / X11 / Wayland lifetimes, events and input. The window-generation token invalidates stale surface acquisitions. `SIR_GRAPHICS_SURFACE_CHANGED` signals resize/recreation; generation-tagged render/backbuffer resources remain in the driver's fence-retired ownership until safe.

## GRDG integration boundary

GRDG will later map canonical engine resource IDs and revisions to existing `sirGraphicsHandle_t` GPU backings. It supplies dependency decisions and multi-queue completion requirements, but **does not** acquire a second physical GPU allocation owner. Renderer/FrameGraph lowers pass/barrier plans to the same command-list API. No GRDG types or forward declarations appear in shared public headers. Implement GRDG provider publication and physical-byte aggregation after the native driver contract is verified.

## Implementation order (one owner per tree)

1. **Shared ABI (this PR):** compile `SIR::Graphics` on Linux/Windows, validate backend selection, stale generations, invalid descriptors and queue submission contracts.
2. **D3D12 workgroup:** replace `SIRD3D12Device`/specialized kernel public entry points with the shared driver/device/commands implementations, retaining native D3D12MA and queue internals. Delete the superseded exposed test-specific interface after all tests migrate; do not add adapter shims.
3. **Vulkan workgroup:** implement the *same* interface using VMA, VkDevice, sync2, timeline completions and the Vulkan feature profile. No separate public renderer ABI.
4. **OpenGL workgroup:** implement the same resource/pipeline/serial command contract and explicit capability negotiation for GL 2.1/3.x/4.x; fixed-function and unsupported compute features follow reported capabilities.
5. **Other DirectX versions:** use exactly this interface, driver-specific offline payloads and capability profiles; no cross-version compatibility adapters.
6. **Integration:** native window ownership, shader cooker, resource/descriptor pools, real draw/present regression and GRDG publication only after the providers have proven resource/completion behavior.

## Acceptance

The always-built `renderer.graphics.interface` test is independent of D3D/Vulkan GPU drivers, Windows SDK or native GPU hardware and runs on Windows/Linux C++17 CI. It covers duplicate/invalid registration, explicit selection without fallback, API-version/feature/software filters, overlarge adapter enumeration, stale handles and completion sets, invalid buffer/texture/pipeline descriptors, incompatible command-list queues and invalid cross-generation waits.

This is **a working, tested common interface and registry**, not a claim that the existing draft D3D12 and Vulkan branches have been migrated. Their native execution acceptance gates remain separate until they implement these shared contracts.

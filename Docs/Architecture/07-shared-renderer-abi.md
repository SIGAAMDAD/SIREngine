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

The version for `SIR_GRAPHICS_BACKEND_OPENGL` negotiates 2.1, 3.3, 4.6 etc. D3D7, 8, 9, 10, 11, 12 are explicitly distinguishable backends. `sirGraphicsCapabilities_t` advertises optional compute, multiple queues, storage, fixed-function, sampler feedback, mesh/ray, HDR and presentation. Drivers must reject unsupported operations explicitly. A legacy API advertises its supported pipeline model(s) and their real limits behind the same binary ABI. Distinct modern, programmable-era and fixed-function lowering paths are required; see the three-era specification linked below.

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
- The existing `SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION` kind is not yet the full historical pipeline contract: fixed-function materials still require the independently tagged lighting, texture-stage, fog, alpha and transform state specified below. More specialized feature passes are negotiated through `nFeatureMask`.

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


## SIR-ABI-001 binary driver boundary (ABI-00 to ABI-03 baseline)

**Roadmap:** [#130](https://github.com/SIGAAMDAD/SIREngine/issues/130). **Native convergence:** [#125](https://github.com/SIGAAMDAD/SIREngine/issues/125), [#126](https://github.com/SIGAAMDAD/SIREngine/issues/126), [#127](https://github.com/SIGAAMDAD/SIREngine/issues/127).

`SIRRenderingDevice.h` remains the canonical C++17 *source* interface and does not promise cross-compiler vtable interoperability. A separate, versioned *binary* module boundary is declared in `SIRGraphicsDriverABI.h` and validated by `SIRGraphicsDriverABI.cpp`. Its required entry-point symbol is `SIRGetGraphicsDriverABI` with signature matching `sirGraphicsABIFactory_t`. The binary contract uses fixed-width integers, size-tagged POD output tables, opaque device pointers, a host allocator/log callback table, and no native SDK types. Device creation returns a separate callback table; all pointers and their lifetimes belong to the returning backend.

The binary graphics ABI remains **v1.0** while its foundational model contracts are completed. The current draft is not a released binary-compatibility promise; adding the planned three-era pipeline contract is part of finishing v1, not a reason to increment it. The size-tagged negotiation rules and mandatory callback validation are retained. The host rejects a missing mandatory callback, incompatible major version, undersized known prefix, invalid device table, invalid format/queue profile, or contradictory advertised capabilities. Negotiation builds into a temporary driver object and publishes only a completely valid table. If a backend has allocated driver context before returning a malformed successful table, its release callback must destroy it. A failing factory must clean its own allocations before returning.

**ABI-00 layout constraints:**
- `sirGraphicsHandle_t` and `sirGraphicsCompletion_t` remain exactly 16 bytes. Resource handles are not native pointers and include resource/device generation; completion tokens include queue and device generation.
- All new binary table fields are fixed-width integers, fixed arrays, callback pointers, or opaque pointers; no C++ `bool`, virtual class, STL/EASTL, allocation ownership object, native Vulkan/DXGI/GL/OS SDK structure or GRDG handle crosses this boundary.
- ABI module loads require matching 64-bit process architecture. The exported callback signature uses the native x64 platform calling convention; explicit 32-bit support is not provided by version 1.
- The backend is the sole owner of device context and native memory. The host manages any allocation it makes via callbacks; `pReleaseDriver`, `pDestroyDevice`, `pShutdown` have distinct ownership responsibilities. The host must keep the dynamic module loaded until driver, devices, callbacks and their outstanding native work have been retired.

**ABI-01 negotiation:** `sirGraphicsNegotiateDriverABI` validates the host and mandatory driver table, invokes the factory, and publishes only complete output. The shared branch now also contains dynamic module loading through `SIRDynamicLibrary`, device ownership and mock DLL/DSO tests. Actual Vulkan/D3D12 exports, native command forwarding and real hardware validation remain open.

**ABI-02 facts, formats and profiles:** `sirGraphicsABIAdapter_t` reports the concrete backend, hardware/software classification, vendor/device ID, stable adapter ID, exposed features/queues and memory sizes. `sirGraphicsABICapabilities_t` describes features **enabled on a created device**, its generation, queue mask and resource limits. `sirGraphicsABIFormat_t` describes per-format optimal and linear usage, and sample counts. `sirGraphicsSelectABIProfile` derives the initial three profiles: `0` headless compute, `1` reduced graphics and `2` modern graphics. These are compatibility classification results, not proof that an attached native driver works. Native code must query actual enabled device features and actual per-format capability bits, rather than copying adapter maxima or optimistic API-version tables.

**ABI-03 conformance contract:** The test target `renderer.graphics.binary-abi` exercises self-contained fake driver negotiation, strict layout and capability/format negative cases. It must be extended with *both* native implementations after their separately owned branches adopt the binary export and the unified `SIRRenderingDevice` contract. Existing Vulkan headless compute and D3D12 WARP/graphics paths do not become conformant merely because the shared test compiles. D3D12's added `sirGraphicsDeviceCreateDesc_t` fields (`pCpuHeap`, API/GPU validation and device fault diagnostics) are still **branch-local drift** and require a separate shared configuration decision in #125/#127. Vulkan's compute-only feature reporting and missing graphics/present path must be handled in #126, not hidden by this ABI layer.

**Testing evidence rule:** A build target or test being committed is *not* proof it executed. Record exact candidate SHA, compiler/toolchain, CMake/CTest commands, unit test output, formatter/AST and platform-native GPU checks separately. Never close native conformance from mock tests alone.

### Binary ABI v1 command/device vocabulary

`SIRGraphicsABIOperations.h` defines binary-facing POD counterparts for pipeline, shader, render-pass, buffer-texture copy, native surface and present descriptors whose source-facing structs contain C++ `bool`. Fixed-width primitive handles, completion tokens, buffer/texture/view/sampler descriptors, and scalar enums reuse the canonical shared definitions.

The v1 device function table covers: device-enabled capabilities; per-format support; memory budget; buffer/texture/view/sampler/pipeline/binding creation; descriptor writes; mapped-range operations; command-context acquisition; ordered command recording (barriers, copies, render passes, pipeline/bind and draw/dispatch); multi-queue submissions with explicit waits; generation-tagged polling and timeouts; fence-gated resource retirement; platform surface create/resize/acquire/present; and shutdown. Operations that an API does not support return `SIR_GRAPHICS_ABI_UNSUPPORTED`; mandatory callback *slots* may not be null.

The `sirGraphicsABICommandList_t` table carries its queue and device generation and is a borrowed backend-owned recording context. The `sirGraphicsABISubmission_t` batch supplies opaque closed command identities plus wait/acquisition arrays; the submit result is a generation-tagged queue completion token. Render submission does not implicitly wait for GPU/CPU work. Resource destruction is deferred by the completion set on `pRetireResource`; physical resource backing stays owned by the native driver.

`sirGraphicsLoadABIModule` uses SIREngine's existing `SIRDynamicLibrary` platform service, looks up the exact binary entry symbol and negotiates into a temporary driver table. Module unload is refused while any device created through the module remains live. Device creation is gated by adapter software policy, required/enabled features, frame capacity and a valid device callback table; failures destroy their temporary device and leave output unchanged. Adapter, device capability and per-format query results are validated before publishing.

The `renderer.graphics.binary-module` test compiles an independent shared-library mock driver and exercises the actual runtime module interface, software-adapter policy, malformed adapter/format/capability output, device creation failure, retained module ownership, command record/dispatch/submit, completion generation, and release. This is **not** a native Vulkan/D3D12 GPU execution claim; those exports remain the responsibility of backend integration in #125-127.

### Unified result/error codes

Binary callbacks propagate the canonical `sirGraphicsResult_t` numbering without a second graphics result enum. The version negotiation error is appended as `SIR_GRAPHICS_VERSION_MISMATCH = 11` (after the existing driver error code 10); `OUT_OF_MEMORY`, `BUSY`, `TIMEOUT`, `DEVICE_LOST` and other preexisting errors retain their existing codes. Unknown provider result numbers map to `SIR_GRAPHICS_DRIVER_ERROR`. The binary device-creation request also transports the explicit `nHeadless` policy as a fixed-width boolean and rejects unrecognized reserved fields.

## Three renderer pipeline-era contracts: required ABI v1 freeze

The authoritative source-controlled specification is [20-renderer-pipeline-eras.md](20-renderer-pipeline-eras.md), tracked in [roadmap #130](https://github.com/SIGAAMDAD/SIREngine/issues/130). It defines **modern**, **programmable-era**, and **fixed-function** models within one graphics module ABI v1. Pipeline model, API execution style (explicit/stateful serial) and software-versus-hardware adapter class are separate negotiated axes. The current 0/1/2 headless/reduced/modern feature-tier heuristic is **not** a pipeline-era selector. The capability-model queries and tagged modern/programmable/fixed pipeline descriptors are planned under ABI-PM01 and must be frozen before native Vulkan/D3D12 reconciliation. Native three-era conformance has not yet been demonstrated.

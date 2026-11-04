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

The authoritative source-controlled specification is [20-renderer-pipeline-eras.md](20-renderer-pipeline-eras.md), tracked in [roadmap #130](https://github.com/SIGAAMDAD/SIREngine/issues/130). It defines **modern**, **programmable-era**, and **fixed-function** models within one graphics module ABI v1. Pipeline model, API execution style (explicit/stateful serial) and software-versus-hardware adapter class are separate negotiated axes. The current 0/1/2 headless/reduced/modern feature-tier heuristic is **not** a pipeline-era selector. ABI-PM01 implements model capabilities, the generation-tagged per-model device query, and mutually exclusive modern/programmable/fixed pipeline recipes in the binary v1 contract. Native driver exports must adopt this exact contract before Vulkan/D3D12 convergence. Native three-era conformance has not yet been demonstrated.


### ABI-PM01 implementation notes

- `sirGraphicsABICapabilities_t::nPipelineModelMask` reports a bit for each supported model (`1U << (nModel - 1)`). `sirGraphicsABIDevice_t::pQueryPipelineModelCaps` queries limits, shader stages/formats, fixed-function options and execution style for the **enabled device**, with an explicit device generation. Neither API version nor the 0/1/2 feature-tier heuristic supplies this value.
- `sirGraphicsABIPipeline_t` carries `nModel` and three mutually exclusive, typed borrowed recipe pointers. The modern recipe requires cooked shader/stage/feature compatibility; programmable requires supported shader profiles and slot limits; fixed-function requires no shader payload and validates lights, fog, texture combiners and bounded stages.
- Host wrappers `sirGraphicsQueryABIPipelineModelCaps` and `sirGraphicsCreateABIPipeline` validate all reported caps and the pipeline before publication, and reject an invalid returned pipeline handle/generation. New `renderer.graphics.pipeline-eras` CTest and extended `renderer.graphics.binary-module` test exercise source validation and native-module function-table negotiation respectively. These tests are CPU mocks, not native API conformance.

### ABI-04 host-side ranked admission (source foundation)

The shared C++ `SIRGraphicsRegistry::selectDriver` and the binary v1 host helper `sirGraphicsSelectABIModuleAdapter` now enumerate eligible adapters rather than selecting the first registration-order match. Both enforce the explicit backend choice without cross-API fallback, reject malformed enumerator counts and unrecognized feature bits, and prefer eligible physical devices over software adapters. Among equally eligible adapter classes, the ranking favors reported feature coverage, dedicated bytes, shared bytes, then stable backend/adapter identity as a deterministic tie-breaker. A software adapter is considered only when `bAllowSoftware` / `nAllowSoftware` is set. No device is created and no heap allocation is performed during selection.

AUTO selection rejects an API-specific minimum version because Vulkan and Direct3D version numbers are incomparable. Callers who specify a minimum API version must also select that backend explicitly. A malformed provider response is a driver error, not an excuse for silent fallback; an unsupported candidate is skipped. The selected output remains unchanged on an unsuccessful binary module selection. The host module selection record contains the exact `nModuleIndex` and full validated `sirGraphicsABIAdapter_t` to use with the existing device-creation API.

This **implements host-side selection policy only**. WARP, lavapipe/LLVMpipe probing, native adapter classification, device creation/retry, working GPU queues and actual software frame output still require corresponding native-driver acceptance. Therefore ABI-04 and ABI-07 are **not** complete.

**ABI-04 host-only test matrix:** `renderer.graphics.interface` verifies physical-vs-software selection, feature and reported-memory ranking, registration-order independence, backend-specific API version admission and invalid provider features. `renderer.graphics.binary-module` uses the independent mock DLL/DSO to test a software adapter followed by integrated/discrete candidates, explicit backend enforcement, feature-gated software fallback, invalid adapter counts and malformed feature masks. These are CPU/provider contract tests, not proof that WARP or Mesa renders a frame. Keep ABI-04 open until native acceptance is demonstrated.

### ABI-02 source-side format and resource admission

Shared host code now queries real device-enabled `sirGraphicsABIFormat_t` capabilities before native graphics pipeline allocation. Every specified color attachment must advertise `SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT`, and a depth attachment must advertise `SIR_GRAPHICS_ABI_FORMAT_DEPTH_STENCIL`, on optimal tiling. Each attachment must admit the requested sample count in its per-format bitmask, rather than relying solely on device-wide maximum samples. Unsupported format/usage pairs return `SIR_GRAPHICS_UNSUPPORTED`. Malformed provider format records return `SIR_GRAPHICS_DRIVER_ERROR`. Both `sirGraphicsCreateABIPipeline` and `sirGraphicsSelectABIPipelineVariant` use this same admission; the selection operation skips unsupported authored alternatives but propagates provider errors.

`sirGraphicsCreateABITexture` is the corresponding no-allocation, failure-transactional host entry point for texture creation. It checks structural texture invariants, type-correct color/depth usages, device-enabled dimensions and MSAA, shader-storage eligibility, each requested format capability (sampled, storage, color, depth, transfer source/destination), optimal-versus-linear CPU memory access and supported per-format sample counts. Only then does it invoke native `pCreateTexture` and verify the returned type/generation before exposing it. The backend retains actual memory/tiling and resource ownership, and may return `UNSUPPORTED` on further driver-specific constraints.

Extended mock DSO conformance tests deliberately vary format attachment support, valid-but-incompatible sample-count masks and malformed format metadata. **This is a portable host admission implementation, not native hardware probing**. ABI-02 remains open until Vulkan/D3D12 (and later GL/legacy providers) report truthful capabilities under real GPU/CPU software devices and pass native tests. The single binary ABI remains **v1.0**.

### ABI-05 source resource admission and budget contract

The host-facing binary v1 helper now validates buffer, texture, view and sampler construction without modifying the driver function table or moving native resource ownership. `sirGraphicsCreateABIBuffer` rejects invalid sizes/alignments/usage, checks that vertex/index, shader-storage, indirect and GPU-upload buffers have the required enabled device features, and validates the returned buffer handle and device generation. `sirGraphicsCreateABIView` accepts only valid texture/buffer resource generations, valid mip/layer ranges, exactly one supported view usage, sensible color/depth format roles, and actual device image-format support where applicable. Raw/typed **buffer** views use the explicit byte-range fields `nBufferOffsetBytes` and `nBufferSizeBytes`, with mip/layer fields zero; texture views use mip/layer fields and leave buffer-range fields zero. The `sirGraphicsViewDesc_t` v1 POD is explicitly 56 bytes on supported 64-bit hosts. Buffer views do not use texture tiling-capability bits; native drivers validate buffer length, stride/format support and resource compatibility. Depth image sampling is explicitly legal when the device reports sampled support. `sirGraphicsCreateABISampler` enforces source sampler validation (including NaN-resistant float checks) and returned handle generation.

`sirGraphicsQueryABIMemoryBudget` reads the existing driver's budget callback into a temporary snapshot and rejects incoherent counters before publication. A zero reported budget is unknown, and the host does **not** assume OS budget estimates are hard allocation ceilings or force eviction. Driver allocation/memory accounting and retirement remain native. Each wrapper leaves the caller output untouched on all failures and propagates `DEVICE_LOST` and provider error codes without collapsing them.

The independent mock native DLL/DSO tests cover valid resources and budgets, stale resource generations, invalid view descriptions, unsupported buffer features, unsupported texture/view formats, malformed budget reporting, and device-loss propagation. This establishes the host-contract foundation only; real Vulkan/D3D12 driver memory/queue/WSI, native GPU tests and branch-wide coverage are outstanding. ABI major/minor remain **1/0**.


### ABI-05 checked queue/completion boundary

Host-side `sirGraphicsAcquireABICommands`, `sirGraphicsSubmitABICommands`, `sirGraphicsPollABICompletion` and `sirGraphicsWaitABICompletion` now validate the **current enabled device capabilities** before invoking the existing v1 native callbacks. Queue availability is a device fact, not inferred from API version or whether the driver advertises graphics/compute features. Acquired command-list tables must be complete, generation-scoped and bound to the requested queue; a malformed native response returns DRIVER_ERROR without publishing the table.

The native submission ABI still receives driver-owned opaque command-context pointers. The checked host submission path validates command-pointer presence and uniqueness, queue admission, size-tagged batch fields, completion wait tokens, and acquired surface-image identities. For consistency with the existing source-side validator, the limits are 64 command lists, 64 waits and eight surface acquisitions per batch. Surface acquisitions require an enabled PRESENT feature on the DIRECT queue. A successful native submit must return a nonzero completion with the current device generation and submitted queue. Poll returns only 0 or 1; a native response outside that range is a driver contract violation. Failed operations never mutate the caller's output; DEVICE_LOST is propagated unchanged.

The new host helpers do not own or copy command-list data and do not assume a particular queue implementation (Vulkan, D3D12, OpenGL or software). Native backends remain responsible for command-list lifetime, actual submission synchronization, single-use acquisition consumption, and GPU/device-loss recovery. The DLL/DSO mock tests cover unsupported queues, malformed tables, stale native generations, invalid submission waits, duplicate command pointers, invalid returned completions and DEVICE_LOST.

**ABI v1.0 remains unchanged**: these are host-only functions layered over the existing native driver function table. ABI-05 is not complete until platform drivers and native state-machine tests pass.


### ABI-05 window/surface lifecycle admission

The host-side binary ABI v1 adds `sirGraphicsCreateABISurface`, `sirGraphicsResizeABISurface`, `sirGraphicsAcquireABISurfaceImage` and `sirGraphicsPresentABISurfaceImage`. No binary driver callback or ABI version was changed. The native driver remains the sole owner of window surfaces, images, synchronization reservations and present semantics; host wrappers only validate and dispatch.

The surface description is size-tagged, carries a supported Win32/X11/Wayland platform, non-null window and nonzero window generation, and bounds the requested backbuffer count, dimensions, present mode, tearing/HDR flags and surface format. Device-enabled PRESENT capability and DIRECT queue are mandatory. HDR requests require HDR_OUTPUT. Format support is validated against the enabled adapter's optimal color-attachment format and non-MSAA sample-1 mask; the driver may reject platform-specific surface support. Surface creation publishes only a valid type/generation handle.

Acquire validates a live surface and checks that the native response references **the same surface handle**, a valid texture handle from the current device, a nonzero one-use acquisition identity and window generation. Present requires the exact current device generation, DIRECT queue completion token and valid surface/image reservation metadata. Validation or native failure never mutates caller-owned result objects; DEVICE_LOST propagates unchanged. Resize does not magically preserve old acquisitions: the driver must invalidate stale image and window generations and enforce its own WSI state machine.

The independent mock driver tests use a synthetic PRESENT device and cover normal create/resize/acquire/present, headless rejection, disabled HDR, malformed dimensions, stale returned surface/image handles, invalid acquisition IDs and device-loss propagation. **Native Vulkan/D3D12 WSI and visual swapchain conformance have not been demonstrated.** ABI-05 remains open pending real backend acceptance.

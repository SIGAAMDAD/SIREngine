# D3D12 headless bootstrap (experimental)

**Branch:** `renderer/d3d12-headless-foundation`  
**Base:** `develop` at `0a7d47de77130b63290ec4950bdf2e257dbfe326`  
**Scope:** Native Windows Direct3D 12 foundation only; no GRDG, Vulkan, frame graph, window manager or shader cooker dependency.

## Implemented bootstrap boundary

- Opt-in `SIR_WITH_D3D12` for Windows, default OFF; Linux default build is unchanged.
- `SIRD3D12Device` owns DXGI factory, explicit hardware-or-WARP adapter, D3D12 device, direct graphics queue, native fence and event.
- Query selected adapter identity and DXGI local memory budget/usage without adding those observations to any GRDG allocation ledger.
- `copyRoundTripForTest` verifies upload -> DEFAULT GPU buffer -> readback with an explicit copy-state transition, queue signal, completion wait and byte comparison.
- The diagnostic fixture uses committed resources deliberately. It is **not** the runtime allocator, frame uploader or GRDG physical-residency implementation.
- The only D3D12 SDK types are in `Source/Renderer/Drivers/D3D12`; cross-driver public headers are unchanged.

## Follow-on integration work (ordered)

1. Freeze the common `SIRGraphicsTypes` / `SIRRenderingDevice` ABI with device-generation handles, typed results and per-queue completion tokens; keep the D3D12 native objects private. The D3D12 owner then adapts this bootstrap into that ABI.
2. Add D3D12MA 3.2.1 through the pinned `ThirdParty/D3D12MemoryAllocator`, Core memory callback metadata, placed-resource heaps, transient rings and exact physical-allocation backing IDs.
3. Add command allocator/list pools, independent compute/copy queues, multi-queue fence sets, state-tracked barriers and deferred destruction that never recycle an in-flight allocation.
4. Add shader-visible descriptor heaps, RTV/DSV storage, root-signature lowering and offline DXIL PSO loading under the shared binding ABI.
5. Add native surface/swapchain once Core Platform's Windows window owner supplies a generation-checked HWND token. Do not introduce a secondary input/event pump.
6. Register GPU-derived resources with GRDG after its device-generation provider identity, multi-queue completion set and single-count physical ledger contracts are stable.
7. Add native WARP correctness, hardware validation/PIX/DRED, real GPU memory benchmarks, device loss and resize tests before treating the driver as render-capable.

## Tests and limitations

```sh
cmake --preset dev -DSIR_WITH_D3D12=ON
cmake --build --preset dev
ctest --preset dev --output-on-failure -R '^renderer[.]d3d12[.]'
```

Windows CI enables the experimental option; the test requests **WARP explicitly** and does not certify physical GPU throughput. Hardware initialization, memory optimizations, presentation, GRDG retirement and enhanced barriers remain NOT RUN. CI should build and run this slice before merge.


## Continuation: D3D12MA and reusable copy command objects

The driver now compiles the pinned D3D12MA 3.2.1 source as an internal library dependency. The native allocator is created after the device and before the first GPU buffer. GPU copy tests use D3D12MA allocation ownership rather than hand-created committed resources. A single direct command allocator and command list are reused after an exact fence completion; each buffer is released only after its associated work finishes.

The diagnostic test performs 64 transfers with distinct data and validates the completed-submission counter. The WARP CI job initializes the pinned submodule, then compiles and runs these checks. A blocked/uncertain native copy retains all allocations and command objects until completion can be established; shutdown deliberately abandons native references if it cannot prove safe retirement.

Remaining: reusable **asynchronous** upload/readback rings, independent compute/copy queues, per-queue completion sets, graphics descriptors, shader pipelines, swapchains, DRED, Core memory metadata callbacks and GRDG backing-ledger integration.


## Independent copy queue and bounded asynchronous transfers

- `SIRD3D12Transfers` owns a real D3D12 COPY command queue, a separate fence and three command allocator/list slots.
- Each slot lazily allocates upload, DEFAULT and readback resources through D3D12MA and reuses them while size permits. Reallocation occurs only after the slot's previous fence is retired.
- Nonblocking `submit` returns a local serial+slot ticket. The fourth concurrent submission fails with an explicit busy result rather than waiting.
- `poll`, bounded `wait`, `receive` and `discard` validate live ticket ownership. A completed slot is reusable only after explicit receive/discard. Stale tickets cannot alias a reused slot.
- The copy queue records upload and readback in separate command lists and submits them with separate `ExecuteCommandLists` calls. This uses Direct3D 12's COMMON state promotion/decay for DEFAULT buffers; COPY queues prohibit resource transition barriers. Separate COPY queue ownership avoids consuming direct-graphics queue time.
- Device shutdown proves the copy queue's latest submitted fence before releasing slots and native allocator backing. A failed signal or unverifiable completion forces deliberate abandonment of native objects to avoid GPU use-after-free.
- WARP tests cover ring exhaustion, out-of-order completion checks, invalid and stale tickets, varying payload sizes, repeated reuse and ring growth.

The transfer queue remains single-caller at the native bootstrap level. Multi-threaded submissions, independently tracked compute workloads, GRDG device generations, a shared renderer completion token, large persistent upload heaps and texture transfers are subsequent integration steps.


## Independent compute queue and multi-queue completion snapshots

The Windows-only bootstrap owns a D3D12 COMPUTE command queue, fence and event separate from the graphics and COPY queues. The compute queue supports a nonblocking native command-list marker with fence-safe command allocator reuse. The marker deliberately records no shader dispatch: pipeline binding and cooked DXIL will be integrated later.

A native `sirD3D12CompletionSet_t` records one device generation plus the latest submitted graphics, compute and copy fence values. `snapshotCompletionSet` captures the state and `pollCompletionSet` checks completion against all three independently owned native fences. Submissions from a prior initialized device cannot satisfy a new generation. A requested future fence is rejected.

Tests exercise overlapping COPY/COMPUTE marker submissions, exact completion snapshots, 64 reusable compute submissions, invalid completion values and stale generations after shutdown/reinitialize. This is still a backend-local completion API; later GRDG/FrameGraph integration will own the renderer-neutral completion contract and GPU-side cross-queue ordering.


## GPU-side COPY to COMPUTE fence dependency

The native D3D12 driver can now submit a COMPUTE-queue marker with a GPU-side dependency on an active COPY transfer. `SIRD3D12Transfers::queryDependency` validates the live serial/slot ticket, returns its submitted COPY fence value and borrows the native fence for immediate use. `SIRD3D12Device::submitComputeAfterCopy` passes that fence to `SIRD3D12ComputeQueue::submitMarkerAfterFence`, which enqueues `ID3D12CommandQueue::Wait` before the compute submission. The CPU does not wait during submission.

The COMPUTE queue retains a COM reference to its dependency fence until its own marker completes. Failed signal or unprovable completion deliberately preserves queued objects and the retained dependency fence. Device shutdown drains the COMPUTE queue before releasing the COPY queue, so a COPY fence cannot be retired while a COMPUTE wait still uses it.

WARP regression coverage executes 48 distinct COPY -> COMPUTE dependency chains with variable payload sizes and readback comparisons. Invalid/forged and retired transfer tickets are rejected; each COMPUTE completion is checked against the COPY fence in the recorded completion set. This establishes native GPU-side ordering but does not dispatch shaders or integrate GRDG resource lifetime ownership.


## COMPUTE -> DIRECT GPU-side dependency and completion retention

The native D3D12 backend now supports a graphics-queue marker dependent on a previously submitted COMPUTE completion set. `SIRD3D12Device::submitGraphicsAfterCompute` checks generation, rejects unsignaled future fence values, borrows the COMPUTE fence through `queryDependencyFence`, and enqueues `ID3D12CommandQueue::Wait` followed by a DIRECT queue `Signal`. Submission does not wait on the CPU. The latest graphics fence is recorded in the returned three-queue completion snapshot.

The DIRECT queue retains an owned COM reference to the COMPUTE fence until its matching graphics fence completes. A second unresolved dependency reports `ERROR_BUSY`; after retirement, its fence object may be reused. A native `waitForGraphicsFence` provides an explicit bounded CPU wait for tests and shutdown. Failure to submit the graphics signal marks the queue unsafe and retains dependent native objects.

Shutdown drains the DIRECT queue before COMPUTE, then COPY; it only releases the device and D3D12MA backing after all queue completions are proven. WARP tests execute 48 COPY -> COMPUTE -> DIRECT chains and compare the original payload against readback. Future fence targets, zero fence targets, stale device generations and null results are rejected.

Actual shader execution, FrameGraph barriers and GPU physical backing-ledger integration are separate milestones.


## Bounded multi-queue resource retirement

The native D3D12 device now maintains a fixed-capacity deferred-retirement list of D3D12MA allocations. The `retireNativeAllocation` API accepts a unique allocation owner and a generation-checked `sirD3D12CompletionSet_t`. Registration transfers ownership only on success; a rejected registration leaves the allocation with its caller. The capacity is 32 allocations for the initial headless bootstrap, with explicit `ERROR_BUSY` when full.

`collectRetiredAllocations` never waits for GPU execution. It checks the recorded graphics, compute and copy fences, releasing each owned allocation only after **all three** reach the registered completion values. A pending entry stays allocated. The collection list is compacted without dynamic allocation. During shutdown, the engine drains DIRECT → COMPUTE → COPY and then releases every registered allocation before destroying D3D12MA. If completion cannot be proven, allocations and their allocator are deliberately abandoned rather than destroyed while referenced by GPU work.

Windows WARP tests cover 32 registered allocations against a COPY → COMPUTE → DIRECT completion set, capacity overflow, invalid and future fence values, stale device generations, reclamation counts, empty collection and uncollected resources on shutdown. These are native backend ownership contracts; GRDG will later supply canonical resource identity, backing-byte accounting, and configurable retirement capacity.

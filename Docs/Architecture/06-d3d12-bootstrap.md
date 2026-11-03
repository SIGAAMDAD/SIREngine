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

# SIREngine D3D12: current driver status and consolidation record

**Source authority:** `features/d3d12-headless-foundation` (PR #33), based on the shared renderer contract merged to `develop` in PR #25. The historical headless journal is `06-d3d12-bootstrap.md`; the acceptance and profiling methodology is `08-d3d12-validation-performance.md`.

## Ownership and implementation inventory

| Owner | Primary responsibility | Current state |
|---|---|---|
| `SIRD3D12Driver` | Register driver and select hardware/WARP; implement shared `SIRRenderingDevice` | Available |
| `SIRD3D12Device` | DXGI factory/device, queue fences, dependencies, native completion and graceful shutdown | Available |
| `SIRD3D12Memory` | D3D12MA owner, sealed transfer backing, optional Core `SIRGeneralHeap` callbacks, native ledger | Available |
| `SIRD3D12Resources` | Typed buffer/texture/view/sampler handles, descriptor spaces, reference tracking and retirement | Available |
| `SIRD3D12Pipelines` | Offline cooked DXIL, PSOs and root signatures | Available |
| `SIRD3D12Bindings` | Fixed-capacity shader-visible binding sets and typed descriptor writes | Implemented and covered by native WARP |
| `SIRD3D12Commands` | DIRECT/COMPUTE/COPY lists, barriers, copies, draw/dispatch, fence submissions | Available |
| `SIRD3D12Surfaces` | Generation-checked Win32 DXGI acquire/resize/present and external backbuffers | Available |
| `SIRD3D12Diagnostics` | Opt-in debug layer, DRED and device health snapshots | Available |
| `Tests/Unit/SIRD3D12Tests.cmake` | Native D3D12 CTest registration and offline DXIL fixtures | Consolidated |
| `.github/actions/setup-dxc` | Shared offline DXC installer for WARP and hardware CI | Consolidated |
| General compute and indexed graphics test fixtures | Cooked DXIL compute and raster regression through shared `SIRRenderingDevice` | Native-only specialized PSO owners retired |

## Handle and lifetime contracts

- Every device uses fixed-capacity, generation-checked native resource handles. Queue context and resource reuse require completed native fence evidence, with no per-dispatch allocation.
- The shared graphics API has no direct dependency on DXGI, Vulkan, the editor, or GRDG. The D3D12 implementation owns native GPU allocations.
- Binding sets use HLSL register spaces for `nSetIndex`, and each declared binding produces one D3D12 root descriptor table. CPU-side view/sampler handles are copied into reserved **contiguous shader-visible ranges** so descriptor arrays remain valid.
- Capacity: 16 live binding sets, 128 descriptors per set, 32 declared shader bindings per pipeline, 128 descriptors per shader-visible heap (including resource views and set ranges). Capacity exhaustion returns `SIR_GRAPHICS_CAPACITY_EXCEEDED`; creation does not grow the heaps.
- Binding-set ownership pins its root pipeline and all bound view/sampler handles until retirement. Recorded command lists pin the binding set until the GPU completion fence retires. Descriptors cannot be updated while recording/submission references remain.
- `sirGraphicsDescriptorWrite_t` addresses a register and array element. Ambiguous same-space register numbers across different binding kinds are rejected at set creation because the portable write structure does not specify a binding kind.
- Source views represent resource subranges. The binding implementation currently requires `nOffsetBytes == 0` and `nRangeBytes == 0` in descriptor writes; arbitrary dynamic descriptor offsets/ranges return `SIR_GRAPHICS_UNSUPPORTED`. Separate view creation supplies the slice.
- Win32 swapchain surfaces own acquisition IDs and imported resources. Their external backing is budgeted, but DXGI remains the native COM owner.

## Authentic consolidated feature history

The six-feature history is reconstructed from **previously committed source trees**, not hypothetical work stages. Every rewritten stage below retains the exact tree SHA of the original state and preserves original ownership attribution:

| Patch | Original committed source | Rewritten commit | Verified tree |
|---|---|---|---|
| D3D12-00 — headless queues, allocator and shared driver ABI | `3942a8455` | `916948b55` | `09f982b3d7458c282a2f428c94530437d6a25a82` |
| D3D12-01 — native resources/descriptors | `72092a1ea` | `7feabff76` | `04f5ef311fed12d097fef88f1da2190f14d98143` |
| D3D12-02 — cooked DXIL pipelines | `dca193002` | `f56af6bbc` | `d52b4d9dd805157e597c3ab231da820cad419643` |
| D3D12-03 — command recording/submission | `e8a2335a9` | `5206bd9bc` | `678c5059dcc5e10a843a9bbd1c76d8688f2bdf79` |
| D3D12-04–05 — DXGI and memory | `fa73476d3` | `61cdf22ee` | `062644e75a6aa04065ea0325531245487bab8097` |
| D3D12-06 — diagnostics/profiling | `fa028fdb0` | `2aca27737` | `04025101d16331d6d2a3e629fddda6efef3f118d` |

Every original stage passed its applicable Windows D3D12 WARP and full-repository CI before the reconstruction. A new branch with the same trees triggers fresh CI. The source transition following D3D12-06 constitutes **new work** with its own commit and tests, not a simulated historical snapshot. Do not fold later work into the historical milestones.

Validation PRs #28–#32 and the historical D3D12 PR #21 are closed. PR #33 is the single active D3D12 feature PR; its branch is `features/d3d12-headless-foundation`. PR #25 shared driver API was merged independently to `develop`, and Vulkan PR #19 is rebased at PR level against `develop`.

## Tests and performance evidence

The authoritative list is `Tests/Unit/SIRD3D12Tests.cmake`. The post-cleanup source passed [nine of nine native WARP suites](https://github.com/SIGAAMDAD/SIREngine/actions/runs/41783132164) and [Windows/Linux native, pinned formatting and Linux profiling CI](https://github.com/SIGAAMDAD/SIREngine/actions/runs/41783132290). It includes native WARP adapter/transfer/graphics, shared API, DXIL pipelines, command recording, Win32 presentation, memory, DRED health/stress, and descriptor binding tests; the benchmark smoke is a separate target. Offline shader code is generated by pinned DXC **only at build time**.

For performance decisions compare same-machine hardware results, not WARP timing to a discrete GPU. The benchmark JSON includes CPU p50/p95/p99, queue submission/fence wait, native GPU timestamps, and memory ledger. A dedicated GPU runner, Debug Layer / DRED modes, induced removal, and PIX/ETW traces are **not** silently marked complete by WARP CI.

## Next implementation and cleanup boundaries

1. **Native test-owner cleanup completed:** The specialized `SIRD3D12ComputeKernel` and `SIRD3D12GraphicsKernel` and their bespoke native driver entrypoints are removed. `SIRD3D12HeadlessTests` retains native queue, transfer, device-generation and deferred-retirement coverage; generalized graphics, shaders and descriptor-bound compute are exercised through `SIRD3D12CommandsTests` and `SIRD3D12BindingTests`. The latter includes 1, 2, 3, 17 and 64 compute group rounds with readback checks. Full WARP CI remains required on this change.
2. **Broader binding support:** Support texture arrays, multiple register spaces, typed CBV/SRV/UAV and sampler combinations, and per-resource view slices as distinct smoke fixtures; design dynamic offsets separately within the portable ABI if required.
3. **Physical GPU acceptance:** Run the dedicated hardware profiling workflow on UMA and discrete devices. Capture a comparative baseline and DRED/TDR diagnostics. WARP CI is correctness evidence only.
4. **Future GRDG integration:** Map canonical resources and physical byte accounting to the GRDG contract when that subsystem is ready; keep D3D12 ownership independent.
5. **Branch hygiene completed:** Six superseded D3D12 refs (`renderer/d3d12-headless-foundation` and the five `renderer/d3d12-*-validation` branches) were deleted after exact-SHA and closed-PR checks. The [one-time GitHub branch-pruning run](https://github.com/SIGAAMDAD/SIREngine/actions/runs/41783485986) succeeded; its temporary workflow was removed from the feature tree. A fresh GitHub branch listing confirms that `features/d3d12-headless-foundation` is the only D3D12 branch. Historical source provenance remains in the six authentic source snapshots referenced above.

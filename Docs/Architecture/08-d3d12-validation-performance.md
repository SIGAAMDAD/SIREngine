# SIREngine D3D12 Phase 06 — Validation and Performance Program

**Source authority:** `renderer/d3d12-headless-foundation`, the native `SIRRenderingDevice` implementation and its DXGI/D3D12MA owners. This document defines repeatable acceptance gates and separates executed checks from hardware-only experiments.

## 1. Goals and invariants

The Phase 06 gate adds opt-in D3D12 debug-layer / GPU-based validation / DRED configuration, structured removal diagnostics, repeated cross-queue correctness testing, and a machine-specific performance harness. None of the diagnostics features are enabled by default. Device-removal extended data is configured before device creation; the DRED setting is reset after the new device is created. The D3D12 debug layer affects the process and cannot be toggled off while active devices remain. Do not collect production performance data with GPU-based validation enabled.

Owner boundaries:
- The native D3D12 device retains sole ownership of its queues/fences; resources and D3D12MA blocks have exactly one native owner.
- The shared graphics ABI remains free of DXGI/D3D12 types. Device-create validation fields are generic optional requests; implementation-specific DRED data is exposed only in the D3D12 backend header.
- Diagnostics capture copies bounded counts and HRESULTs from DRED without retaining pointers into the driver's private lists.
- Fault handling rejects new work after real device loss and does not retire in-flight native backing without completion proof.
- Debug tools do not inject a second window message pump or reimplement the platform owner.
- GRDG provider IDs and byte-ledger mapping remain separate work.

## 2. Executable correctness suite

| Gate | Binary / entrypoint | Inputs and assertions | Runtime |
|---|---|---|---|
| Native adapter, transfer and graphics | `renderer.d3d12.warp-headless` | Adapter identity, UMA/staging selection, buffer retirement, compute dispatch, offscreen triangle output | WARP CI |
| Driver registration and type safety | `renderer.d3d12.shared-api` | Capabilities, handle generation, zero/future tokens, unsupported API contracts | WARP CI |
| DXIL and PSO setup | `renderer.d3d12.pipelines` | Offline DXIL validity, graphics/compute PSO creation, conflicting root layouts, stale handles, bounded capacity | WARP CI |
| General native commands | `renderer.d3d12.commands` | Three queue command pools, buffer/texture copies, barriers, indexed raster, roundtrip bytes, capacity and double-submit rejection | WARP CI |
| DXGI presentation | `renderer.d3d12.surface` | Window tokens, acquired backbuffer use, present, stale acquisition, resize, view retirement | WARP CI |
| Allocator integration | `renderer.d3d12.memory` | Resident quota, D3D12MA heap-block ledger, resource retirement, invalid CPU heap configuration | WARP CI |
| Phase06 diagnostics + stress | `renderer.d3d12.diagnostics` | HRESULT translation, invalid configuration, healthy snapshot, 64 chained COPY→COMPUTE→DIRECT submissions and deterministic 4 KiB roundtrip | WARP CI |
| Performance harness smoke | `renderer.d3d12.bench-smoke` | 16 measured samples + 4 warmups; native queue, allocation, readback and timestamp probe, machine-readable results | WARP CI |
| Extended queue stress | `SIRD3D12DiagnosticsTests --cycles 1024` | Greater-than-capacity command-list reuse over repeated cross-queue fences without stale references | Manual profile |
| Debug + DRED opt-in | `SIRD3D12DiagnosticsTests --debug --dred --cycles 32` | Graphics Tools availability, debug information queue, DRED configured before device creation; expected healthy device | Manual, Graphics Tools |
| GPU-based validation | `SIRD3D12DiagnosticsTests --gpu-validation --dred --cycles 32` | Slow, optional GPU validation of resource/descriptor states | Manual, Graphics Tools |
| Physical GPU | `SIRD3D12DiagnosticsTests --hardware --cycles 1024` | Identical correctness tests on hardware, fails when a hardware adapter is unavailable | Dedicated runner |

Each test must return a nonzero status on data corruption, failed queue submission, bad synchronization, or invalid expected results. Opt-in Graphics Tools modes return 77 if the requested API is not installed (CTest can display SKIP, not PASS). A hardware run must never silently substitute WARP.

**Negative tests to extend later:** independently injected device-removal reason and forced native queue failures, missing pages / corrupt descriptors in an isolated sacrificial device, DXGI occlusion/fullscreen/HDR/tearing matrix, multithreaded use from actual supported entrypoints, and precise DRED breadcrumb/page-fault capture on a removed physical device. Tests that risk TDRs should run only on a dedicated disposable runner, never ordinary PR CI.

## 3. Performance harness: methodology and output

Executable: `SIRD3D12Benchmarks`. It has no Google Benchmark dependency and can run on the native CMake/FASTBuild D3D12 target. `--adapter warp|hardware`, `--iterations 1..4000`, `--warmup 0..96`, `--output <json>`.

All CPU durations use Windows QueryPerformanceCounter converted with the observed QueryPerformanceFrequency, and collect mean, minimum, maximum, p50, p95 and p99 in nanoseconds. GPU work is synchronized correctly but its timestamp duration excludes CPU wait time. Retain raw configuration, adapter/vendor/device ID, UMA status, dedicated/shared bytes and D3D12MA native byte ledger in each JSON artifact.

| Metric | Measurement and interpretation | CI threshold |
|---|---|---|
| `buffer_create` | 4 KiB GPU_LOCAL allocation creation, after warmup and D3D12MA reuse; resource retirement is outside interval | Numeric sanity, no threshold |
| `submit_empty` | CPU submission call latency, DIRECT queue, real native commands/fence | Numeric sanity |
| `submit_and_wait` | CPU submit through complete fence wait; includes OS scheduler wakeup and queue latency | Numeric sanity |
| `copy_256k_roundtrip` | 256 KiB COPY submit through CPU-visible readback completion; verify the copied payload | Numeric sanity + integrity |
| `gpu_timestamp.copy_gpu` | Eight D3D12 timestamp pairs, each spanning 32 native 64 KiB copy commands; Direct queue frequency converts ticks to ns | Recorded when supported; `available=false` otherwise |
| `memory` | Native heap-block commitment/peak and DXGI-reported residency/budget | Invariants, not absolute allocation targets |

Use `--iterations 384 --warmup 32` in the manual RelWithDebInfo profile. PR CI uses a short Debug-profile smoke and writes JSON; its results are **not** comparative performance acceptance. Neither a single WARP value nor a machine with a different GPU/driver establishes a regression.

### Comparison protocol

1. Same hardware, GPU driver, power settings, OS build, display mode, graphics options, compiler, build preset and source SHA for each candidate and baseline.
2. Collect at least 10 independent process runs (not only 10 in-process iterations), with 384–2048 iterations each. Let resources settle before sampled work, fix the benchmark's byte counts and inputs, and record the actual adapter IDs.
3. Compare median-of-run p50/p95/p99 and GPU-timestamp medians; flag sustained `>10%` p50 or `>20%` p95 regressions for investigation, requiring reproducibility rather than failing arbitrary hosted runners.
4. Compare WARP, one UMA iGPU and one discrete GPU separately. Physical GPU throughput and dedicated/shared residency must never be conflated with software WARP results.
5. Track CPU and GPU work per operation, native D3D12MA block bytes, independent readback/upload mapped sizes, resource recycling, per-queue fence waits and command allocator capacity. Inspect ETW/GPUView or PIX traces when an outlier is reproducible.
6. For full renderer performance, introduce separately controlled mesh/terrain/lighting/streaming scenes **after** descriptor-binding and GRDG presentation integration, with CPU/GPU timestamp queries around explicit passes.

## 4. CI / runbook

Normal PR: `.github/workflows/d3d12.yml` configures D3D12, builds all D3D12 test executables and the benchmark harness, runs `ctest --preset dev --output-on-failure -R '^renderer[.]d3d12[.]'`, and uploads `out/Nightly/Benchmarks/d3d12-warp.json`.

Manual profiling: `.github/workflows/d3d12-performance.yml` selects `warp` on `windows-2022` or `hardware` on a dedicated Windows runner labeled `windows-d3d12-gpu`. It configures `profile` (RelWithDebInfo), executes the extended correctness test and benchmark, and preserves a machine-specific JSON artifact. A missing hardware runner is **not** an executed hardware test.

For local Windows runs:

```powershell
cmake --preset profile -DSIR_WITH_D3D12=ON
cmake --build --preset profile --target SIRD3D12Benchmarks SIRD3D12DiagnosticsTests
ctest --preset profile --output-on-failure -R "^renderer[.]d3d12[.]"
# Run the resulting SIRD3D12Benchmarks.exe with --adapter hardware --iterations 384 --warmup 32 --output results.json
```

### Acceptance labels

- **PASS:** exact source SHA; native compile/link; functional D3D12 WARP tests; source formatting; result JSON has valid numeric fields; no crashes, data mismatches, or invalid lifetime transitions.
- **UNRUN:** physical hardware, optional debug/GPU validation/DRED (unless explicitly exercised), real device loss, vendor-dependent page fault attribution, PIX/ETW captures, full-frame terrain/lighting, and comparative performance baselines.
- **REJECT:** missing output artifacts, forged hardware labeling, source changes after evidence, disabled tests disguised as PASS, or resource release without GPU completion proof.

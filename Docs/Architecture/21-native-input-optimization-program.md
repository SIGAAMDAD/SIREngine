# SIREngine native input optimization and verification program

**Status:** Implementation program and measured slice 1; hardware, rendering and scheduling phases remain open.  
**Source authority:** `SIGAAMDAD/SIREngine`, `feature/native-input-foundation`, baseline `a5577e7747263aecaafd39549e4652a61b13d350`; target remains `Source/Core/Platform/Input/`.  
**Platform contract:** Native Win32/Raw Input/GameInput/XInput/HID on Windows, native Wayland/XInput2/evdev/hidraw on Linux, SDL3 only on approved other operating systems. No superseded input compatibility API.

## Objective and optimization policy

Optimize all layers where equivalent-workload evidence supports an improvement, including nanosecond CPU paths. Make gains additive while maintaining exact button/axis transition semantics, deterministic simulation, bounded memory, device generations and controller output ownership. Input-latency improvements must be evaluated through the rendering/presentation timeline, not inferred from event-function microbenchmarks.

The reference architectures remain UE5 ApplicationCore device/platform ownership, Source Engine polled state with tick transitions, Activision's GDC 2019 controller-to-display scheduling work, Halo Infinite fixed-step and presentation alignment, NetherRealm deterministic input history and native OS API documentation. These are reference implementations, not a claim of binary or source compatibility.

## Baseline and acceptance measurements

- Existing full CI for `a5577e7`: Windows MSVC, Linux GCC, formatting, and Google Benchmark jobs passed; existing native input benchmark medians include frame publication at 32 devices 511.56 ns, single-control batch 519.41 ns, 32-device lookup 28.13 ns, profile lookup across 32 profiles 11.23 ns. The exact CI runner is not guaranteed to remain equivalent.
- New local focused before/after profiling uses the same compiler flags, source test harness, machine, workload, 80,000 ticks per trial, nine trials, and median with range. Local results are comparable **only within this environment**, not directly with GitHub runner values or physical input latency.
- Mandatory checks for every slice: C++17 GCC/Clang and Windows MSVC when impacted, source-formatted with pinned clang-format 18.1.8, focused behavior and negative tests, ASan/UBSan, full CMake/CTest where runnable, regression benchmark comparisons, then non-force GitHub publication and same-SHA CI confirmation.
- Performance gates: investigate any repeatable regression in a supported common workload, even when small. Do not automatically fail noisy shared-runner measurements on a single threshold crossing; inspect variability and isolate compiler/toolchain/power/load changes. Preserve entire before/after JSON samples, source hashes, flags and timing methodology. No unverified speedup claims.
- Physical acceptance gates: separate tests for USB/Bluetooth devices, real 125–8,000 Hz input, Windows GameInput history, XInput2/Wayland pointer capture, OS suspend/recovery, 60/120/144/240 Hz monitors, Vulkan/D3D12 presentation, end-to-end physical LED/photodiode timing.

## Dependency-ordered engineering waves

### Wave 1 — optimize core CPU and memory traffic (implemented in candidate)

1. Keep the fast single-change path and transactional multi-change batches.
2. Track which device-state records changed since **each of the two publication buffers** was last used. Publish only those device records; preserve a fast contiguous-copy path for the all-dirty case. Reset transient presses/releases/motion only for devices that emitted such data. A single dirty bit per tick is insufficient for ping-pong frame publication.
3. Replace the per-slot descriptor copy loop with a registry-owned metadata snapshot to fill device IDs, connected flags and user pairing in one pass, without copying complete device descriptors on every tick.
4. Linux evdev: bulk-read 64 reports per syscall with a strict 512-event per-device drain budget; preserve `SYN_REPORT`, cross-read partial reports and `SYN_DROPPED` resynchronization.
5. Add deterministic 4,000-tick sparse/dense device-state and alternating-buffer tests, a pipe-backed Linux batch test measuring actual read invocations, and Google Benchmark dirty-device variants.

### Wave 2 — capture timestamps and time provenance

Extend the input event and collector contract with receipt timestamp, native timestamp and clock domain, source ordering, and simulation consumption tick. On Linux request a monotonic evdev event clock when supported. On Windows retain source GameInput and Raw Input timestamps with explicit conversion/receipt fallback. Add clock-reset, overflow, and disorder tests. Integrate with `SIRPlatformTime` and only record hardware timestamps when actually provided by the native source.

**Wave 2 implementation progress (partial):** `sirInputEvent_t` now stores source and engine-receipt nanosecond timestamps separately, identifies the source clock and native-time validity, and remains a fixed 48-byte record. A frame publishes its monotonic simulation cutoff and oldest/newest event receipt ages without scanning all events. Linux evdev requests `CLOCK_MONOTONIC` through `EVIOCSCLOCKID` and carries the `SYN_REPORT` timestamp into the normalized batch only when the ioctl succeeds and the timeval is valid; otherwise it uses engine receipt time without claiming hardware provenance. All active backends use the same service time contract, and no old-interface adapter exists. Windows GameInput source timestamps, renderer-stage timing, and true hardware-to-photon measurements remain future work. The simulation cutoff here is the input publication instant, not a predictive presentation deadline.

### Wave 3 — native high-rate acquisition

Implement Win32 `GetRawInputBuffer()` batching after safely handling the current `WM_INPUT`; use preallocated buffers and benchmark 1–8 kHz mice and mixed button/motion order. Add GameInput history (`GetNextReading`) and device arbiter before introducing multi-API duplicate input. On Linux use batched evdev and a ready-fd/event-loop owner, then XInput2 raw mouse and Wayland relative pointer/pointer constraints/text protocols. Do not introduce SDL3 runtime links on Windows or Linux.

### Wave 4 — deterministic cutoff and presentation feedback

Separate native acquisition from simulation-frame publication. Pin exactly one canonical ordered event stream per physical device, preserve short press/release pairs, and publish immutable tick state. Implement a renderer-owned adaptive presentation predictor with measured CPU/game/render/GPU/present stage durations, deadline margin, prediction resets and frame-ID linkage. Use DXGI waitable swapchain/latency controls and supported Vulkan present timing/wait extensions. Include native frame-pacing hooks for available Reflex/Anti-Lag integrations without binding input acquisition to vendor runtimes.

### Wave 5 — late visual input and renderer integration

Introduce an explicitly authorized presentation-only camera input view sampled after the deterministic simulation cutoff. Keep networking/physics/gameplay inputs unchanged and replayable. Benchmark camera-to-photon motion and controller/button-to-action latency with both fixed and variable-rate rendering. Measure motion-to-photon tail percentiles and missed-present rate under contention.

### Wave 6 — further hot-path data layout and controller I/O

Benchmark typed keyboard/pointer/gamepad SoA state, smaller or dynamically precommitted event capacity, hardware profile lookup and device indexing, D-pad/axis normalization, haptic output batching and queue priorities, and controller profile cooking. Adopt a change only after matched measurements; include high-device-count and all-dirty negative workloads. Preserve no heap allocations in recurring hot paths and central NMD memory attribution as allocator integration becomes available.

### Wave 7 — end-to-end latency validation

Instrument complete `hardware event -> OS read -> canonical event -> simulation cutoff -> render submit -> GPU complete -> presentation` span. Use GDC-style instrumented button plus photodiode/high-speed recording for physical-to-pixel acceptance. Report p50/p95/p99/max with devices, drivers, refresh rate, renderer and workload. Keep the replay trace format versioned and redact typed text by default.

## Integrate → profile → regressions → patch → push loop

1. **Pin source:** Read branch head, source and ownership SHA, compiler/tool versions and stable reference benchmark JSON. Refuse stale parent or missing build contracts.
2. **Integrate one dependency-complete wave:** Implement production code, consumers, tests, CMake and diagnostics under current `Source/Core/Platform` owner. No old-API adapters.
3. **Profile under matched conditions:** Repeated trials with median, minimum, maximum, p95/p99 when sampled; preserve high-rate and all-devices-dirty cases. Profile CPU, memory, syscall count, and when hardware exists, latency end-to-end.
4. **Check correctness/regressions:** Run unit, deterministic soak, sanitizer, format/AST, and native platform build gates. If a workload regresses, use the first real diagnostic or trace, patch the exact implicated path, and rerun its affected tests and benchmarks. Revert unproven speculative optimizations.
5. **Publish:** Create a commit with the verified parent and only task-owned paths; push non-force; verify remote branch/ref and PR diff; read the resulting same-SHA GitHub CI. Correct reported failures and repeat the loop. Do not classify compiler-only tests as physical hardware acceptance.
6. **Advance:** Carry forward validated baselines and source identity to the next wave. Leave explicit unrun hardware/renderer gates in the PR and this document.

## Primary research sources

- Activision / Akimitsu Hogge, GDC 2019, *Controller to Display Latency in Call of Duty*: https://media.gdcvault.com/gdc2019/presentations/Hogge_Akimitsu_Controller_to_display.pdf
- 343 Industries / Daniele Giannetti, GDC 2022, *One Frame in Halo Infinite*: https://media.gdcvault.com/GDC%2B2022/Speaker%2BSlides/One%2BFrame%2Bin_Giannetti_Daniele.pdf
- NetherRealm, GDC 2018, *8 Frames in 16ms*: https://media.gdcvault.com/gdc2018/presentations/Stallone_Michael_8FramesIn16ms.pdf
- Epic Games, *Low Latency Frame Syncing*: https://dev.epicgames.com/documentation/en-us/unreal-engine/low-latency-frame-syncing?application_version=4.27
- Microsoft, *GetRawInputBuffer* and GameInput reading APIs: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getrawinputbuffer and https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-readings
- Linux kernel, *Input event codes and evdev*: https://docs.kernel.org/input/event-codes.html
- Khronos Vulkan present wait/timing extension specifications: https://registry.khronos.org/vulkan/

# SIR-INPUT-001 — Native Input Completion Roadmap

**Tracking issue:** [#101](https://github.com/SIGAAMDAD/SIREngine/issues/101)
**Canonical PR:** [#22](https://github.com/SIGAAMDAD/SIREngine/pull/22)
**Branch:** `feature/native-input-foundation`
**Original synchronization:** `develop` `51666fadadf8112b3b2d5bf919ba11dde55d931b` merged into input `9470d7aef933ecab28ba1469789ed6ac17bff6bf` at `d5c718c8bdee2d2c7181fd59111885e2c258b936`. Branch was zero behind `develop` when synchronized.

> The GitHub issues are the detailed task contracts. This document is the durable dependency index and acceptance policy. None of the tasks are complete because this roadmap was published.

## Architecture in scope

- `SIRInputService` is the sole orchestration/write boundary; `SIRInputDeviceRegistry` is the authoritative physical identity and generation owner. All native collectors normalize into the same C++17 bounded, single-writer frame state.
- `SIRInputFrameState`: 32 devices, 2048 event slots, ordered press/release, immutable tick snapshots and explicit overflow/resync. Runtime hot state has no recurring heap allocation.
- `SIRInputActions`: 128 action/user entries, 16 context/user slots, 256 bindings; deterministic action mappings, contexts, chords, deadzones and serializable profiles.
- Native Windows: Raw Input and GameInput v1/XInput baseline. Native Linux: evdev/X11/XInput2 baseline. Wayland and per-physical controller arbitration remain unaccepted. SDL3 restricted to optional future other-OS support.
- Text/IME, UI dispatch, haptics, cooked profile, logical replay and bounded input history are implemented foundations, awaiting real client/runtime/hardware acceptance.
- Renderer/presentation owns all visual prediction, GPU timing and late camera view. Gameplay, physics and networking consume deterministic simulation cutoff state. Input history is not a world checkpoint system.
- Compiler/style: CMake-only, C++17, repository formatting rules and clang-format 23.1.5. No retired Godot-style bridge or backwards compatibility input API.

## Dependency-indexed implementation packages

| ID | GitHub issue | Wave | Deliverable | Dependencies |
|---|---|---|---|---|
| NI-00 | [#102](https://github.com/SIGAAMDAD/SIREngine/issues/102) | R0 | Exact-head integration and compiler/AST certification | — |
| NI-01 | [#103](https://github.com/SIGAAMDAD/SIREngine/issues/103) | R0 | 100% source-instrumented executable coverage | NI-00 |
| NI-02 | [#104](https://github.com/SIGAAMDAD/SIREngine/issues/104) | R1 | GameInput/XInput per-physical arbitration | NI-00 |
| NI-03 | [#105](https://github.com/SIGAAMDAD/SIREngine/issues/105) | R1 | Win32 HID and Raw Input completion | NI-02 |
| NI-04 | [#106](https://github.com/SIGAAMDAD/SIREngine/issues/106) | R1 | Wayland native seat/window/relative pointer | NI-00 |
| NI-05 | [#107](https://github.com/SIGAAMDAD/SIREngine/issues/107) | R1 | Linux udev, evdev and hidraw identity | NI-00 |
| NI-06 | [#108](https://github.com/SIGAAMDAD/SIREngine/issues/108) | R1 | X11/XInput2 hardware acceptance | NI-00 |
| NI-07 | [#109](https://github.com/SIGAAMDAD/SIREngine/issues/109) | R1 | USB/Bluetooth/suspend device lifecycle | NI-02, NI-03, NI-04, NI-05, NI-06 |
| NI-08 | [#110](https://github.com/SIGAAMDAD/SIREngine/issues/110) | R1 | Bounded acquisition/recovery diagnostics | NI-03, NI-04, NI-05 |
| NI-09 | [#111](https://github.com/SIGAAMDAD/SIREngine/issues/111) | R2 | Action context/user binding completion | NI-00, NI-02, NI-05 |
| NI-10 | [#112](https://github.com/SIGAAMDAD/SIREngine/issues/112) | R2 | Editor/UI/scripting/IME consumers | NI-00, NI-04, NI-09 |
| NI-11 | [#113](https://github.com/SIGAAMDAD/SIREngine/issues/113) | R2 | Cooked profiles and calibration | NI-05, NI-09 |
| NI-12 | [#114](https://github.com/SIGAAMDAD/SIREngine/issues/114) | R2 | Haptic effect hardware qualification | NI-02, NI-05, NI-07 |
| NI-13 | [#116](https://github.com/SIGAAMDAD/SIREngine/issues/116) | R3 | Native time provenance and simulation cutoff | NI-00, NI-08 |
| NI-14 | [#117](https://github.com/SIGAAMDAD/SIREngine/issues/117) | R3 | Renderer presentation timing/predictor | NI-13 |
| NI-15 | [#118](https://github.com/SIGAAMDAD/SIREngine/issues/118) | R3 | Presentation-only late camera input | NI-13, NI-14 |
| NI-16 | [#119](https://github.com/SIGAAMDAD/SIREngine/issues/119) | R4 | Logical action replay and rollback integration | NI-01, NI-09, NI-13 |
| NI-17 | [#120](https://github.com/SIGAAMDAD/SIREngine/issues/120) | R4 | Memory accounting and matched optimization | NI-00, NI-08, NI-11 |
| NI-18 | [#121](https://github.com/SIGAAMDAD/SIREngine/issues/121) | R4 | Physical input-to-pixel p50/p95/p99 latency | NI-07, NI-12, NI-14, NI-15, NI-17 |
| NI-19 | [#122](https://github.com/SIGAAMDAD/SIREngine/issues/122) | R5 optional | SDL3 adapter for future non-Windows/Linux OS | NI-00, NI-09, NI-12, NI-13 |
| NI-20 | [#123](https://github.com/SIGAAMDAD/SIREngine/issues/123) | R5 | Final Windows/Linux exact-SHA release acceptance | NI-01, NI-07, NI-08, NI-09, NI-10, NI-11, NI-12, NI-13, NI-14, NI-15, NI-16, NI-17, NI-18 |

## Work ordering and parallel seams

1. **R0, first:** Certify the synchronized baseline (NI-00), then close all locally reachable coverage branches (NI-01). Do not report a green current HEAD based on old GameInput-era CI.
2. **R1, native platform acquisition:** Windows NI-02→NI-03; Linux NI-04/NI-05/NI-06 can proceed once NI-00 is stable. NI-07 requires both platforms, and NI-08 depends on the bounded raw backends. The registry and service remain single-writer contracts; avoid parallel edits of those headers without a frozen API handoff.
3. **R2, user-facing control:** NI-09 action/pairing, NI-10 editor/UI/IME, NI-11 profile/calibration and NI-12 haptics. Native controllers supply low-level device data but cannot mutate authoritative action or editor state directly.
4. **R3, timing/presentation:** NI-13 engine/source clocks and simulation cutoff, NI-14 graphics/present prediction, NI-15 late visual-only camera input. Renderer does not alter authoritative input frames.
5. **R4, deterministic/network and optimization:** NI-16 action replay/history checkpoint seam, NI-17 memory/cache workload measurements, NI-18 physical-to-photon validation.
6. **R5, release:** NI-20 final exact-source acceptance closes the mandatory Windows/Linux track. NI-19 is future optional SDL3 portability and explicitly does **not** block NI-20.

## Final acceptance gates

**SOURCE / CI:** Pin branch and upstream SHAs; authentic compiler commands/Clang 23 AST for all touched native input translation units; Linux GCC/Clang C++17 and Windows MSVC with pinned GameInput SDK; CMake configure/build, CTest and GoogleTest integration; source style/clang-format 23.1.5; ASan/UBSan; exact-SHA workflow runs. Demand 100% executable line/branch/function coverage on locally compilable input production code, report each TU and platform-specific exclusions explicitly. No skipped reachable branches, inaccurate coverage from mocks or stale CI evidence.

**BEHAVIOR / DETERMINISM:** Correct identity arbitrator, user pairing and generation retirement, no duplicate physical events, preservation of same-tick transitions, transactional overflow/resync, unique action mapping, correct focus/IME semantics, haptic zero on disconnect, stable little-endian SIRA/SICP/SIRP and replay/history digests. Test malformed buffers, timestamp resets, stale callbacks and per-backend failure cases.

**PHYSICAL ACCEPTANCE:** Real Win32 Raw Input/GameInput/XInput/HID and Linux X11/XInput2/Wayland/evdev/hidraw where implemented; USB/Bluetooth controllers, high-rate mouse, focus/capture, suspend/resume, IME, haptics, hardware vendor/driver versions, monitor refresh and presentation modes. A missing physical device yields NOT_RUN, not PASS.

**PERFORMANCE:** Five or more matched repetitions per workload and full raw output. Capture CPU nanoseconds/microseconds and allocator counters for zero, sparse, dense, 32-device, high-rate pointer, bulk evdev and rumble paths. For end-to-end latency use physical button-to-photodiode/high-speed footage on Vulkan and D3D12, reporting p50/p95/p99/max, missed presents, hardware and OS. No cross-runner speedup claims.

**RELEASE:** NI-20 cannot close until the cumulative branch is current with `develop`, compiler + coverage + sanitizer + native hardware checks on exact final SHA, artifact provenance and PR review are complete. Continue using PR #22 as canonical work branch unless the owner explicitly changes the integration authority.

## Evidence state at authoring

The historical GameInput build passed four CI jobs; subsequent history and merge changes are not automatically accepted. The synchronization merge `d5c718c8` was created and GitHub reported PR #22 mergeable, ahead by 38, behind by zero. Its CMake/CTest, clang-format 23.1.5, AST/coverage and physical acceptance are NOT_RUN in this publication. Roadmap files and issue creation likewise do not validate production implementation.

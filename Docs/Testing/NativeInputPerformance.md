# Native input verification and performance contract

**Scope:** `Source/Core/Platform/Input/` and its Windows/Linux backends, in the same CMake, CTest, GoogleTest, and Google Benchmark build as other SIREngine Core tests. The engine test binary and benchmark binary are not shipped.

## Unit/contract coverage

- `core.platform.input`: device ownership and generation checking; multi-backend rejection; profile matching and validation; frame transitions and focus/device loss; Linux evdev normalization and available X11 event translation.
- `core.platform.input.stress`: reference-model comparison for ordered events, held/pressed/released bits, normalized analog axes, timestamps, monotonically increasing event sequences, 32-device capacity, duplicate physical identities, 8-user assignment limits, generation reuse, vibration routing, transactional invalid requests, exactly-full queue behavior, global overflow resynchronization, pointer saturation, and published-frame lifetime.
- `core.platform.input.soak`: optional deterministic 30,000-tick run with 320 input requests per tick (9.6 million requests); use in nightly and sanitizer builds. Fixed PRNG seed `0x9e3779b9` is printed with event totals to make failures reproducible.
- All stress tests are headless and run using a first-party mock `SIRInputBackend`, without privileged device access, desktop sessions, or SDL3. They exercise the same service, registry, and frame-state translation units linked by `SIRCore`.

The default stress test runs 300 ticks * 96 requests/tick. The extended soak is opt-in using `-DSIR_INPUT_SOAK_TESTS=ON`, so interactive builds remain fast. The nightly workflow passes this option on Windows and Linux profiles, Release, ASan and UBSan configurations. CTest labels differentiate input and longer soak tests; failure exit status is authoritative.

## Commands

```sh
cmake --preset dev -DSIR_INPUT_SOAK_TESTS=ON
cmake --build --preset dev
ctest --preset dev -R '^core[.]platform[.]input' --output-on-failure

cmake --preset asan -DSIR_INPUT_SOAK_TESTS=ON
cmake --build --preset asan
ctest --preset asan -R '^core[.]platform[.]input' --output-on-failure

cmake --preset profile -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
ctest --preset profile -R '^profiling[.]input[.]registration$' --output-on-failure
bash Build/Scripts/RunGoogleBenchmarks.sh input-profile
```

`cmake --preset asan` is a Linux-only preset. Windows verification uses the native CI compiler matrix; the synthetic tests do not replace physical input and window-loop testing.

## Google Benchmark coverage

`SIRInputBenchmarks.cpp` registers independent controlled workloads for:

- frame publication with 0, 1, 8, and 32 active device slots;
- single digital transitions and ordered control batches of 2, 8, 32, and 128 physical transitions;
- raw pointer-motion queues of 125, 500, 1,000, and 2,000 samples per tick;
- device resolution with 1, 8, and 32 registered devices;
- controller mapping lookup with 1, 8, and 32 profiles;
- generation reuse through connect/disconnect per tick.

Benchmark fixture initialization is outside the timed iteration. A benchmark that cannot initialize or emit/publish its requested workload fails through `SkipWithError`. The frame and device cases expose object-size counters; sample/batch cases report items processed so throughput can be derived. Run with matched compiler flags, core count, power mode, workload and repetitions for meaningful comparisons. No absolute nanosecond threshold is enforced on shared CI runners.

`Build/Scripts/RunGoogleBenchmarks.sh` already runs five repetitions and preserves JSON in `out/Nightly/Benchmarks`; the existing `GoogleBenchmarkCompare.cmake` processes matching measurements for nightly reporting. The test `profiling.input.registration` validates that the new input benchmarks are actually linked and visible to Google Benchmark.

## Evidence and future hardware acceptance

Record compiler/SDK version, OS and branch commit, test/benchmark output, runner characteristics and sample count. For performance regressions compare like-for-like JSON entries; investigate consistent median degradation rather than treating noisy single CI measurements as product defects.

The headless stress suite cannot certify GameInput, HID, Wayland, XInput2 raw motion, USB/Bluetooth controllers, evdev permissions, real haptics, or physical polling-to-frame latency. Those require separate native-system and hardware-in-loop validation. Multithreaded access to published frames is not a claimed contract; the current input owner and readers use the documented single-thread tick lifetime.

## Hot-path optimization regression

`SIRInputFrameState::postChanges()` uses a validated direct path for one control change; multi-change reports retain the transactional scratch-state preflight. The deterministic stress model exercises both cases, and `SIRBenchmarkInputControlBatch/1` isolates the fast path in the regular Google Benchmark output. Validate the single-change and batch cases together before accepting future optimizations.

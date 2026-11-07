# SIRTaskScheduler verification and profiling

## Regression coverage

The native CTest executables are registered in `Tests/Unit/CMakeLists.txt`:

| Test | Scope |
| --- | --- |
| `core.jobs.scheduler` | Admission, handle generations, dependency propagation, priority and cancellation |
| `core.jobs.graph` | Deterministic and randomized RAW/WAR/WAW ranges, cycles, execution summaries |
| `core.jobs.provider` | Provider ownership, safe-point handoffs, pending completion, shutdown |
| `core.jobs.fences` | Fence ordering, rollback, finalizer retry, cancellation, quiesced failure |
| `core.jobs.stress` | 4 simultaneous producers, repeated 64-pass DAGs, wide cancellation cascades, priority fairness, 64-ticket fence bursts, SIR metadata stability |

`core.jobs.stress` is a separate CTest target with a 90-second timeout. Native CI executes it on Windows and Linux alongside the existing suites. The consolidated qualification workflow runs native ASan+UBSan tests on Linux. Concurrency race detection with ThreadSanitizer is a separate local gate; it is **not** implied by a passing ASan/UBSan job.

### Targeted local checks

```sh
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset dev
ctest --preset dev -R '^core[.]jobs[.]' --repeat until-fail:20 --output-on-failure
```

For sanitizer builds, use fresh build directories rather than mixing sanitized and unsanitized object files. Repository CMake supports `SIR_ENABLE_ASAN` and `SIR_ENABLE_UBSAN`; the workflow `.github/workflows/qualification.yml` runs the required sanitizer jobs.

## Profiling workloads

All cases use the engine's Google Benchmark target `SIRCoreBenchmarks` and output standard JSON (the existing profiling CI artifact). They are independent measurements, not a score to optimize without inspecting the profile:

| Benchmark | Independent variables | What is timed |
| --- | --- | --- |
| `SIRBenchmarkTaskSubmitPumpRelease` | Workerless baseline | Submit, execute and release on owner thread |
| `SIRBenchmarkTaskBatch` | 1/8/32/64 tasks | Workerless dependency chain |
| `SIRBenchmarkFenceAcknowledgement` | One ticket | Provider pump and mock fence acknowledgement |
| `SIRBenchmarkWorkerBatch` | 1/2/4/8 workers; batch 64/256; 0/64/2048 operations | Full admission-to-terminal CPU cycle with workers |
| `SIRBenchmarkConcurrentProducers` | 2/4/8 producers, 2/4/8 workers | End-to-end concurrent submission, waking, work and release |
| `SIRBenchmarkWorkerFanInOut` | 1/4/8 workers; 32/128/256 children | One root, parallel children and joined completion |
| `SIRBenchmarkFenceBurst` | 32/128 tickets per fence | Deferred provider queue admission and batched acknowledgement |
| `SIRBenchmarkWorkerDispatchLatency` | 1/4/8 workers | Submit-to-callback-start p50/p95/p99 in nanoseconds |

`dispatch_p50_ns`, `dispatch_p95_ns`, and `dispatch_p99_ns` are per-batch percentiles averaged over the benchmark's batches. The callback timestamp overhead is included; sort/percentile processing is excluded from the timing. Producer benchmarks include producer wakeup cost. They are **not** GPU, audio, network, or Jolt device-fence latencies.

### Reproduce

```sh
cmake --preset profile -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
# Locate the binary in out/<platform>/profile/Tests/Benchmarks/ for the configured host.
# Run with a stable CPU governor, no competing build, and comparable host configuration:
SIRCoreBenchmarks --benchmark_filter='SIRBenchmark(Worker|Concurrent|Fence|Task)' \
  --benchmark_min_time=0.1s --benchmark_repetitions=7 \
  --benchmark_report_aggregates_only=true --benchmark_out=sir-jobs.json --benchmark_out_format=json
```

Record host CPU, logical cores, compiler/optimization flags, sanitizer status, test commit SHA, affinity, and CPU frequency scaling. Compare identical task workloads and concurrency levels. The scheduler currently synchronizes worker-local queues with one global mutex: collect CPU contention and cache-miss evidence before selecting lock-free deques or an alternative executor. Google Benchmark JSON provides medians/dispersion, but diagnosing contention requires an actual CPU profiler such as Linux `perf` or a platform-equivalent sampling profiler.

Do not interpret low owner-pumped microbenchmark nanoseconds as evidence of low p99 latency under contention. Do not establish arbitrary hard-coded performance failure thresholds without a matched baseline and stable runner hardware.

# GRDG and Core infrastructure validation matrix

## Scope and source of truth

Canonical PR: [#23](https://github.com/SIGAAMDAD/SIREngine/pull/23). The head of the same integrated commit is authoritative for GRDG, Core Jobs, native Windows/Linux I/O, page backing, child processes, filesystem and thread controls.

**No legacy scheduling compatibility layers.** The only execution owner is Core `SIRTaskScheduler`. The GRDG writer owns publication and staging, native OS objects own file and mapping handles, and Core Memory owns page commitment.

## New CTest coverage

`resources.integration.stress`, compiled from `Tests/Unit/SIRInfrastructureIntegrationTests.cpp`:

| Scenario | Assertions |
| --- | --- |
| Positional/mapped read matrix | 2 modes × 5 file offsets × 5 lengths = **50** independent GRDG publications, including unaligned mapped offsets, subpage payloads, 64 KiB and 256 KiB transfers |
| Core Jobs saturation | One-slot scheduler rejects a second load without leaking staging, cancellation releases it, and subsequent load succeeds |
| Four-worker integration | **12 simultaneous file transfers** across both native I/O modes, all validated against deterministic source bytes |
| Resource version turnover | **256 sequential revisions**, alternating read modes and offsets, with verified payloads and bounded retained allocation |
| Invalid requests and wrong-writer calls | Null/empty paths, invalid alignment/mode, empty/out-of-range intervals, missing dependency descriptors, wrong-thread start/poll, task handle validation |
| Resource ownership | After each scenario: no active scheduler handles and zero outstanding GRDG allocations |

Existing CTest suites in the same CI matrix additionally cover resource graph correctness, derived-provider fences and retirement, task DAG, scheduler stress, native platform APIs, memory page-backing and OS failure injection.

`jobs-sanitizer.yml` repeats native Core Jobs and GRDG I/O integration targets under ASan/UBSan **5 times**. `allocator-sanitizer.yml` repeats allocator, backing, staging and GRDG integration targets under sanitizers **3 times**.

## New optimized Google Benchmark cases

Benchmark executable: `SIRCoreBenchmarks` linked directly to `SIR::Resources`. Added **22 scenarios**, with five repeated trials in the existing optimized Linux profiling job:

| Family | Workload sizes | Measures |
| --- | --- | --- |
| `SIRBenchmarkGRDGNativeReadAt` | 4 KiB / 64 KiB / 1 MiB | Native positional reads into preallocated destination |
| `SIRBenchmarkGRDGMappedCopy` | 4 KiB / 64 KiB / 1 MiB | Map, copy entire view, and unmap; same byte work as positional path |
| `SIRBenchmarkGRDGLookup` | 32 / 256 / 1,024 resources | Read scope + hash lookup + read-only resolve |
| `SIRBenchmarkGRDGPublishReplace` | 64 B / 4 KiB / 64 KiB | GRDG staging, canonical publish, revision replacement, retired record collection |
| `SIRBenchmarkGRDGFileIngest` | 4 KiB / 64 KiB / 1 MiB × two modes | **Complete** file open → Core task admission → native read/map → GRDG publication → collection |
| `SIRBenchmarkGRDGPageRetireCancel` | 1 / 16 pages | No-syscall pending-page retirement/cancellation |
| `SIRBenchmarkGRDGPageDrainRecommit` | 1 / 16 pages | OS decommit drain and reacquisition |

All I/O microbenchmarks use deterministic identical bytes and hot files for comparable steady-state workloads. They do **not** claim cold-storage latency. Per-iteration setup includes the operations listed under each workload and excludes the one-time fixture file creation. Throughput is reported as bytes/s where meaningful; source data correctness is validated.

## Benchmark evidence and regression gates

`Build/Scripts/RunGoogleBenchmarks.sh` publishes the existing Google Benchmark JSON artifact, and `Tests/Profiling/ValidateSIRGRDGBenchmarks.py` fails CI when any of the **22 required median scenarios** is missing, has a runtime error, or reports invalid timing.

For a genuinely matched historical baseline, compare with:

```sh
python3 Tests/Profiling/ValidateSIRGRDGBenchmarks.py \
  out/Nightly/Benchmarks/ubuntu-24.04-pr.json \
  --baseline path/to/verified-same-machine.json \
  --max-regression-percent 20
```

The comparison rejects incompatible host/CPU/build metadata. A >20% higher median triggers a failure **only when the caller supplies a comparable baseline**. There is no invented fixed nanosecond cutoff and no benchmark performance verdict until the current CI run produces results.

## Instrumented compiler coverage

`.github/workflows/grdg-coverage.yml` builds the same native source through the
Ninja tooling preset with GCC `--coverage`, runs all relevant GRDG/Core Jobs/
Platform/memory CTests, and gathers per-translation-unit line and branch
coverage using `gcov --json-format --branch-probabilities --branch-counts`.

`Tests/Profiling/CollectSIRGRDGCoverage.py` verifies that nine required
production translation units actually produced instrumented object data and
writes machine-readable `grdg-coverage.json` plus an accompanying Markdown
report as an Actions artifact. The job rejects missing objects, incomplete
collections and zero executed statements. The initial instrumented run measured **3,081 of 3,460 executable lines
(89.05%)** and **1,880 of 2,748 branches (68.41%)** across nine production
translation units. CI now requires **at least 80% executable-line coverage
and 60% branch coverage separately for each of the nine translation units**.
This rejects low-coverage changes even when the aggregate remains high.

`profiling.grdg.benchmark-validator` exercises seven positive/negative
regression checks on the benchmark JSON validator, including missing cases,
invalid observations, host mismatch and a measured >20% time regression.

## Remaining measurement work

Line/branch coverage is recorded from instrumented GCC objects in the dedicated coverage job. Test count and instrumentation percentage are reported separately. Multi-worker contention, cold disk I/O, memory pressure, actual GPU/physics/audio provider timings and long-running stress under real asset streams require dedicated, controlled experiments. These are separate acceptance findings, not implied PASS by unit-test success.

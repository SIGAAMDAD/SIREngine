# SIREngine scheduler experiment program — executable S9 foundation

**Authority:** `feat/grdg-task-scheduler-20261103`; do not infer a universal winner from synthetic tests.

**Implemented in this patch:** six additional benchmark families, deterministic native
correctness regression, machine-readable experiment manifest, experiment runner,
matched-run comparison, and synthetic policy ranking. Existing S8 benchmarks, sanitizer
workflow, provider tests and profiling remain intact.

## Experiments already runnable

| ID | Runner filter | Variables | Measures |
| --- | --- | --- | --- |
| E02 | `SIRBenchmarkExperimentContention` | workers 1/2/4/8; shared atomic off/on | Callback-caused contention vs executor overhead |
| G01 | `SIRBenchmarkExperimentGranularity` | 0/1/4/8 workers; chunks 16/64/256/1024; work 0/8 rounds | Work per admission, chunk crossover, metadata |
| Q06 | `SIRBenchmarkExperimentLocalSteal` | 2/4/8 workers; 64/128/256 spawned children | Actual local queue publication, steals, throughput |
| D06 | `SIRBenchmarkExperimentDagShape` | 0/4/8 workers; chain, fan-out, binary tree | Scheduling + dependency completion |
| D08 | `SIRBenchmarkExperimentGraphCompile` | 32/128 passes; disjoint, readers, overlapping writes | Compilation-only, excluding graph construction |
| SYS | `SIRBenchmarkExperimentReplay` | 6 synthetic kernels; worker 1/4/8, chunk 32/128 | Per-kernel granularity and worker crossover |

The synthetic replay kernels cover regular renderer preparation, terrain/HLOD-like
transformations, irregular AI/GOAP-like work, physics-like numerical work, streaming
byte transformations, and short audio-preparation/events work. Each replay uses the
same 8,192 deterministic inputs and validates the same result checksum across
all worker/chunk variants. These are **calibrated synthetic categories**, not actual
backend traces and not an assertion about subsystem performance.

`E02` separates a cache-line-isolated per-task result from the old shared atomic
counter. `Q06` creates children *inside a worker callback*, which exercises worker
local queues (the earlier owner-produced worker benchmarks showed zero steals).

## Run and archive experimental evidence

```sh
cmake --preset profile -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
# Substitute the discovered native SIRCoreBenchmarks path for BINARY.
python3 Tools/Profiling/SIRRunSchedulerExperiments.py BINARY \
  --manifest Tests/Benchmarks/SIRSchedulerExperiments.json \
  --output-dir out/Experiments/baseline
# After changing ONE algorithm, rerun on the SAME machine and workload.
python3 Tools/Profiling/SIRRunSchedulerExperiments.py BINARY \
  --manifest Tests/Benchmarks/SIRSchedulerExperiments.json \
  --output-dir out/Experiments/candidate
python3 Tools/Profiling/SIRCompareBenchmarks.py compare \
  out/Experiments/baseline/E02-callback-contention.json \
  out/Experiments/candidate/E02-callback-contention.json \
  --output out/Experiments/e02-diff.json
python3 Tools/Profiling/SIRCompareBenchmarks.py rank \
  out/Experiments/candidate/SYS-synthetic-replay.json \
  --output out/Experiments/synthetic-rankings.json
```

The runner records benchmark executable SHA-256, source HEAD when available,
host/OS/CPU count, manifest SHA-256, full argv and execution status. It does
not create an unattended background task. `--only` runs selected IDs.

`compare` requires matched benchmark names and checks reported CPU count,
frequency and scaling information when available. Differences are real-time
median changes converted to nanoseconds, **not confidence intervals**. For
statistical decisions, interleave independent runs (ABBA or randomized pair
order), summarize distributions across run files, and use paired bootstrap
confidence intervals. Preserve raw JSON; do not rank configurations across
different kernel profiles, payload counts, resource constraints or machines.

## Control gates

- Native C++17 Windows and Linux builds and all `core.jobs.*` tests pass.
- New `core.jobs.experiments` includes chunk-count/checksum equivalence,
  worker 0/1/4 behavior, DAG parent-before-child, local spawning, active-count
  release and stable metadata after initialization.
- Correctness must precede timing: no dropped accepted tasks, invalid handle,
  stale execution, leaked provider ticket or untracked allocation.
- Track p50/p95/p99 latency, useful throughput, scheduler CPU time, lock
  wait/hold duration, steals, cache misses, context switches, memory bytes,
  worker park time and task-graph critical-path duration as **separate** data.
- Performance choices need dedicated hardware and matching job payloads;
  shared GitHub CI is a smoke, not a ranking authority.
- Validate each profile against the frame/subsystem-specific hard deadline and
  memory budget, then select only non-dominated, correctness-qualified options.

## Follow-on implementation waves (not included in S9 foundation)

| Wave | Experiment | Comparative variants and acceptance |
| --- | --- | --- |
| S10/Q01-10 | Queue algorithms | Current global lock, independent worker locks, preallocated Chase–Lev, sharded MPSC/MPMC ingress; correctness + lock counters + identical workloads |
| S10/W01-08 | Wakeups/placement | Condvar, semaphore/futex, limited spin, wake-one, cascading wake; ready-to-start p99, joules, bounded idle CPU |
| S10/D01-10 | Dependency scheduling | Mutex, atomic indegree, batch completion, successor bypass, retained graph; correctness and graph critical path |
| S10/G01-08 | Task partition | Fixed, adaptive, guided, range stealing and task fusion; equal useful operations and checksum |
| S10/M01-10 | Memory/provider | AoS/SoA, local free caches, tombstones, hash dedupe, fence batch; lifetime/ABA and measured bytes |
| S11 | Cross-library comparisons | enkiTS/oneTBB with matching engine adapter semantics and memory accounting |
| S12 | GRDG real resource benchmarks | Leases, interval hazards, persistent graph versions, resource publication and provider pins; GRDG branch integration required |
| S13 | Native subsystem kernels | Jolt barrier, renderer pass recording, Terrain3D/HLOD, FMOD bank prepare, input/event, I/O, GOAP; exact source and workload fixtures required |
| S14 | Simultaneous interference | Combat, high-speed traversal, asset loading, memory pressure, editor import; whole-frame p99 and deadline misses |
| S15 | Hardware-specialized policies | Controlled 4/6/8/16-core hosts; Win/Linux; hybrid cores; profile-specific Pareto selection and holdout validation |

## Specialist policy matrix — hypothesis register

| Workload | Candidate policies | Hard gate |
| --- | --- | --- |
| Renderer preparation | Static locality chunks, dynamic stealing, hybrid, critical-path priority | CPU frame critical path, GPU idle, safe-point ownership |
| Terrain/HLOD/VT | Region/page/range work; predictive priority; adaptive chunks; throttled staging | Visible late pages and peak streaming budget |
| GRDG | Retained DAG, incremental hazard compilation, atomic dependencies | Version/reader lease safety and publication |
| Jolt physics | Native Jolt pool, shared SIR adapter, participant cap, barrier variants | Fixed step p99 and Jolt barrier safety |
| Animation | Per-character, static contiguous ranges, guided range stealing | Identical output pose and frame deadline |
| GOAP/perception | Per-agent, fixed batches, adaptive cost-aware chunks | Stale planning age and simulation deadlines |
| Audio preparation | Dedicated service worker, pooled jobs, bank batch, control batches | No real-time mixer deadline regression |
| Network | Dedicated polling, MPSC packet batches, shared processing | Packet p99 and loss under overload |
| File/streaming | Blocking I/O workers, OS async I/O, staged read/decompress/publish | Asset-ready p99 and memory budget |
| Events | Direct, deferred single, batched MPSC, priority routing | Handler ordering and event-to-consumer p99 |
| Editor | Priority UI with worker quotas vs throughput | Interactive latency under asset import |

Do not introduce a speculative runtime dependency or force one scheduler algorithm
onto every subsystem. The minimum deliverable for each wave is actual source,
repeated matched measurements, adversarial tests and a candidate/result ledger.

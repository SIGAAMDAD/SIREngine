# Configuration I/O integration: tests and performance

**Status:** IMPLEMENTATION SLICE PREPARED LOCALLY; native source files are not yet committed.  
**Remote baseline:** `SIGAAMDAD/SIREngine@feature/config-parsing` at `5ad54bb88e3a724d1f7425d20627ba92bf5472a4`.  
**Scope:** Existing CSV/TSV reader/writer, borrowed source/sink stream wrappers, SIR-managed allocator budgets, Linux/Windows native files, atomic file output, engine-heap integration tests, deterministic adversarial tests, Google Benchmark registration.

## Source/ownership integration

- `SIRConfigMemoryScope`: independent document/scratch requested-live budgets backed by explicit SIR `sirContainerAllocator_t` descriptors. `SIRContainerHeapAdapter` can provide those descriptors from `SIRGeneralHeap` and `SIRMemoryManager`; lifetime is caller-owned, no implicit allocation fallback.
- `SIRConfigBoundedSource` and `SIRConfigBoundedSink`: callback propagation and input/output byte budgets; source callback supports streaming to the existing CSV/TSV reader.
- `SIRPlatformFileReader` and `SIRConfigFileSource`: native synchronous Linux/Windows file input into the config callback contract.
- `SIRPlatformFileWriter` and `SIRConfigFileSink`: native same-directory staging and explicit commit/abort; Linux flushes file and containing directory, Windows flushes before `MoveFileExW` replacement. The caller must abort on a failed bound or parser write; an output sink cannot silently roll back a committed file. A directory durability failure *after* rename can return an error even though the replacement became visible.
- `SIRDelimitedWriter`: emits buffered 4 KiB chunks to the caller's sink rather than a separate callback for every delimiter, quote and field. The complete row's output budget is checked before the first callback. No heap allocation was added for staging.

## Native CTest registrations

| CTest name | Tested behavior | Local status |
| --- | --- | --- |
| `core.config.delimited` | CSV/TSV parse/write, multiline quotes, chunk splits, row/column limits, output cap, OOM, sink callback batching | PASS (Linux Clang ASan+UBSan and optimized) |
| `core.config.io-and-memory` | Independent budget descriptors, invalid callbacks, bounded source/sink, native file input, atomic file abort/replace; checks active under `NDEBUG` | PASS (Linux Clang ASan+UBSan and optimized) |
| `core.config.adversarial` | 3,000 deterministic CSV/TSV write-read trials with quotes, CR/LF, tabs and embedded NUL; fault injection on allocation requests | PASS (Linux Clang ASan+UBSan and optimized) |
| `core.config.heap-integration` | Actual `SIRMemoryManager` + two domain-separated `SIRGeneralHeap`/`SIRContainerHeapAdapter` contexts, owner tags, parser memory accounting | NOT_RUN (requires full SIREngine source checkout) |

`Tests/Unit/Config/CMakeLists.txt` owns registration of these executables; `Tests/Benchmarks/CMakeLists.txt` compiles the Google Benchmark source into the existing `SIRCoreBenchmarks` when the profiling dependency is enabled.

## Performance suite registration

The added `SIRConfigBenchmarks.cpp` registers these Google Benchmark entry points:

- `SIRBenchmarkConfigBufferParse`: 4, 64, and 1024 KiB synthetic CSV buffers;
- `SIRBenchmarkConfigStreamParse`: 64 KiB with callback chunks of 1, 64 and 1024 bytes, and 1024 KiB with 1024-byte chunks;
- `SIRBenchmarkConfigWrite`: records containing fields of 32, 256, 4096 and 8192 bytes with injected quotes/delimiters;
- sampled Google Benchmark counters: processed bytes, scratch peak requested bytes, SIR memory-domain committed bytes, and sink callbacks per row.

Existing `Build/Scripts/RunGoogleBenchmarks.sh` captures JSON snapshots and repetitions. Full suite compilation/execution is NOT_RUN until the source is published and full CMake/FASTBuild plus pinned Google Benchmark are available.

## Measured standalone microbenchmark — local only

Environment: Linux x86-64 KVM container (5 exposed CPUs, AMD EPYC 9V74 reported by `lscpu`), Clang 17.0.0 with `-O3 -DNDEBUG`, synthetic repeated 30-byte-ish CSV records, mock callback allocator; no engine heap in this standalone run. Single run on 2026-11-03; measurements are environment-specific and not a release speed target.

| Test | Input/output throughput | Rows or sink callbacks | Memory |
| --- | ---: | ---: | --- |
| 1 MiB-class buffer parse, repeated 15 times | 136.12 MiB/s | 524,280 rows | 256 B scratch peak |
| One-byte stream chunks, repeated twice | 82.82 MiB/s | 69,904 rows | 256 B scratch peak |
| 64-byte stream chunks, repeated 15 times | 141.86 MiB/s | 524,280 rows | 256 B scratch peak |
| 1024-byte stream chunks, repeated 15 times | 143.11 MiB/s | 524,280 rows | 256 B scratch peak |
| Buffered CSV writer, 100,000 rows | 289.07 MiB/s | 100,000 calls, 1.00 callback/row | Fixed 4 KiB stack buffer |

This experiment is a functional measurement of the current implementation, not a controlled benchmark against the prior writer. The Google Benchmark suite is the authoritative comparison facility once running in the native SIREngine build.

## Reproducible target validation

```sh
# Requires SIREngine's CMake >=4.4.5 and pinned build dependencies.
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset dev
ctest --preset dev --output-on-failure -R '^core[.]config[.]'

cmake --preset asan -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset asan
ctest --preset asan --output-on-failure -R '^core[.]config[.]'

cmake --preset profile -DSIR_WITH_GOOGLETEST=ON -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
bash Build/Scripts/RunGoogleBenchmarks.sh config-linux-profile
```

The local environment has CMake 3.31.6 and clang-format 23.1.5, whereas the repository requires CMake 4.4.5 and clang-format **22.1.5**. Exact formatter/publish, full native SIREngine build, Windows, full memory-manager and Google Benchmark runtime gates remain NOT_RUN. The implementation source patch must not be published to the GitHub branch until the exact pinned formatter is available and checked. No source commit to GitHub is claimed here.
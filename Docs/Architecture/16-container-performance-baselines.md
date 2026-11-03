# 16 - SIREngine container performance baselines

**Source reference:** PR #8 at `6da0a5cc4830bc9c27a1eaf283bce275ec50a329` before the optimization patch. **Platform:** AMD EPYC 9V74 virtual Linux, Clang 17, C++17, optimized builds. CPU timings are medians of nine trial runs per operation, not hardware-cycle measurements. Memory calls were measured through the explicit domain-descriptor allocation functions. Baseline and candidate used the same source workloads and toolchain. The original four-way library comparison covered 38 container families and 397 serial measurements, but cannot be used as a direct benchmark of SIREngine code.

## Original vendor signals

| Workload | Measured implementation | Median CPU / memory characteristic |
| --- | --- | --- |
| General 32,768-element hash-map hit | Boost flat hash | 3.38 ns/lookup; ~544 KiB live table bytes in the four-way run |
| Fixed hash lookup (2,048 records) | EASTL fixed hash | ~2.13 ns/lookup, zero heap, ~128.1 KiB inline table object |
| Fixed map alternative | UE5 `TMap` fixed allocation | Zero heap, ~72.5 KiB inline table object in the tested policy |
| SPSC queue transfer | Boost lockfree SPSC | 8.82 ns/message in the benchmark; bounded memory footprint |
| Paged random access | UE5 `TChunkedArray` | ~0.55 ns/read in the dedicated stable-address test |

Values reflect different data layouts and test sizes; compare only like-for-like operations and capacities.

## SIREngine prior defects and source observations

- Sanitizer-confirmed dangling-references: `SIRVector::tryEmplaceBack` and `SIRInlineVector::tryEmplaceBack` relocated live elements before consuming aliased constructor arguments. `SIRFlatHashMap::insertInternal` staged aliased mapped values, but not aliased keys, before rehash. Regression tests now cover these cases.
- Original hash indexing masked `std::hash<K>` low bits directly. 8,192-entry stride-65,536 lookups took >2.4 microseconds each under deliberate clustering.
- Incremental `tryResize` used exact-capacity growth: 8,192 vector allocations, 8,176 inline-vector allocations and 1,024 bit-vector allocations in measured append-style resize workloads.
- Repeated 32-byte appends to `SIRString` made 8,192 reallocations for 8,192 operations. The fixed `SIRString` uses geometric growth while retaining 23-byte inline content storage.
- The original 64-bit first-set-bit search used a shift loop; compiler bit-scan produced ~0.62 ns per word against the source loop's ~19 ns on random one-hot positions.

## Matched local before/after measurements (2026-11-02 task continuation)

| Workload | Baseline | Revised implementation | Note |
| --- | ---: | ---: | --- |
| Stride-256 hash lookup, 8,192 keys | 47.774 ns | 2.024 ns | 23.6x faster; robust mixing instead of low-bit masking |
| Stride-65,536 hash lookup, 8,192 keys | 2,411.625 ns | 2.777 ns | ~869x faster on adversarial structured IDs |
| Sequential hash lookup, 8,192 keys | 1.408 ns | 2.314 ns | Regression from mixing and reduced bucket locality |
| Incremental vector resize, 8,192 ints | 8,192 allocations | 19 allocations | Cumulative requested bytes: 134,234,112 → 139,440 |
| Incremental inline vector resize, 8,192 ints | 8,176 allocations | 16 allocations | Cumulative requested bytes: 134,233,568 → 124,528 |
| Incremental bit-vector resize, 65,536 bits | 1,024 allocations | 14 allocations | Cumulative requested bytes: 4,198,400 → 36,656 |
| String 8,192 × 32-byte append | 8,192 allocations | 23 allocations | The resulting live capacity grows from ~256 KiB exact to ~312 KiB geometric |
| String append elapsed time | ~2,813 ns/op | ~12 ns/op | Roughly 240x faster in the repeated-append benchmark |

**Interpretation:** Hash distribution robustness is substantially improved, but randomizing previously sequential bucket layouts increases CPU work and reduces cache locality on favorable key distributions. Benchmarks with the actual GRDG IDs, deletion patterns and runtime allocator are needed before choosing a different hash kernel. The new vector fast path was split from slow growth handling to restore release-build inlining; individual microbenchmark numbers are sensitive to compiler inlining and benchmark code generation.

## Current acceptance and remaining work

- Clang/GCC C++17 tests, sanitizer regressions, randomized hash differential tests and injected allocation failure are required on the final source tree.
- The pinned clang-format 22.1.5 check, native CMake 4.4.5 + FASTBuild matrix, Windows/MSVC build and integrated NMD accounting cannot be inferred from the isolated Linux compiler runs.
- Preserve durable findings and reusable tests in `Docs/Architecture/` and `Tests/Unit/`, rather than adding raw AST dumps, benchmark binaries, `.csv` trials, source archives or ZIP snapshots to Git.

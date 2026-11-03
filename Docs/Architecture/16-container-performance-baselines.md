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

## Iterative hot-path optimization (PR #8 continuation, 2026-11-02)

Source changes: hole-based priority heap sifting, one-multiply 64-bit hash finalization, and overwrite-specific trivial byte growth for `SIRString`. No container capacity/layout changes were made in this pass. Existing allocation-domain ownership and rollback contracts remain in force.

Paired timing on AMD EPYC 9V74 Linux, Clang 17, C++17, `-O3 -march=native -DNDEBUG`, pinned to CPU 1, three alternating process pairs with 11 timed samples after four warmups per process. Values are medians of process medians, in nanoseconds per operation. Both builds used the same test loops and allocation callbacks.

| Workload | Before | After | Saved per operation |
| --- | ---: | ---: | ---: |
| Priority heap, random push/pop | 32.04 ns | 30.15 ns | 1.90 ns |
| Priority heap, sorted push/pop | 29.70 ns | 22.01 ns | 7.69 ns |
| Flat hash map random successful lookup | 3.70 ns | 3.20 ns | 0.50 ns |
| Flat hash map random unsuccessful lookup | 4.32 ns | 3.60 ns | 0.73 ns |
| Flat hash map 64-bit stride-65536 lookup | 2.42 ns | 1.79 ns | 0.64 ns |
| Flat hash map 64-bit stride-2^48 lookup | 2.23 ns | 1.65 ns | 0.58 ns |
| String 32-byte append to growing 256 KiB string | 29.91 ns | 24.57 ns | 5.34 ns |
| Vector 65,536-element growing append | 1.56 ns | 1.57 ns | No material improvement; left unchanged |

The same three primary features were also faster with generic Clang `-O2` without `-march=native`. Tests include random/sorted/reverse/duplicate heap ordering, 64-bit high-bit hash keys, self-aliasing, allocation failure, exception rollback and leak checks. Pinned clang-format and native Windows/Linux CI are the publication acceptance gates.

**Discarded experiments:** branch-only ring wrapping (regression), compiler popcount substitution (near noise floor), fused heap pop (regression), reordered map lookup (mixed results), 7-bit fingerprint metadata (insufficient net gain), and trivial-copy relocation (no material gain). Preserve only the successful algorithm changes. Once deltas approach 0.01-0.1 ns with overlapping trial ranges, treat them as unresolved timing noise, not validated speedups.

## Memory-density and reclamation pass (2026-11-02)

**Source:** `feature/core-containers`, parent `bd33494c60771823e094b2d59e8cbf76d6532d11`. **Platform:** 64-bit Linux, Clang 17/GCC 14.2 C++17, explicit callback allocator reporting exact requested bytes (not RSS or allocation overhead).

**Associative tables:** The duplicated allocation-base pointer was removed from `SIRFlatHashMap` and `SIRFlatHashSet`, reducing each instance from 56 to 48 bytes. Both tables now offer `tryShrinkToFit()`: live entries are transactionally rehashed into a smaller capacity after removal, with allocation failure preserving the old table. Empty tables release all owned allocation. `SIRHashProbe` supports either one byte per bucket (fast, default) or packed two-bit states (`bCompactControl = true` in the final template argument), without separate allocations or hidden system-heap fallback. The compact policy shares the same lifetime, tombstone, rehash and rollback implementation.

| 32,768 occupied `uint32_t` keys | Default | Packed states | Saving |
| --- | ---: | ---: | ---: |
| Map of `uint32_t` to `uint32_t`, heap | 589,824 B | 540,672 B | 49,152 B (8.3%) |
| Set of `uint32_t`, heap | 327,680 B | 278,528 B | 49,152 B (15.0%) |
| Map after removing 28,672 entries + shrinking, heap | 73,728 B | 67,584 B | Compared with initial allocation: 87.5% and 87.5% respectively |
| Set after removing 28,672 entries + shrinking, heap | 40,960 B | 34,816 B | Compared with initial allocation: 87.5% and 87.5% respectively |

**String:** `SIRString` now stores its 24-byte inline array and heap pointer in a union, with the supplied allocation descriptor, byte length and capacity in separate fields. The measured 64-bit object falls from 48 to 40 bytes (16.7% smaller), still holding strings up to 23 bytes plus NUL without heap storage. Added no-allocation moves, explicit shrink-to-inline/size, and footprint inspection. Repeated 32-byte appends ran at 25.20 ns before and 18.84 ns after this patch in a paired, 11-sample, three-process trial on AMD EPYC 9V74 / Clang 17 `-O3 -march=native` (6.36 ns saved per append). These figures describe the exact workload and compiler, not a platform-independent speed guarantee.

**Other containers:** Explicit release of empty bounded queue/ring storage and shrink-through adapters for priority heap, stack, and bit-vector; logical zeroing remains distinct from storage release. `SIRBitVector::clearAndRelease()` resets the logical bit count to zero, whereas `clear()` retains the bit count and zeroes values.

**CPU tradeoff:** Default byte-per-bucket hash probing remains untouched apart from removal of the redundant object pointer; paired default lookup measurements ranged from -0.10 to +0.02 ns benefit, overlapping run-to-run variation. The compact policy performs extra bit operations per probe and was slower on some measured lookup workloads (~1 ns), so it is explicitly opt-in for memory-constrained domains. Repeated deletes alone retain capacity; callers request reclamation at an appropriate lifetime boundary. Shrink can temporarily require an additional allocation before releasing the old table; callback allocation refusal rolls back without publishing partial data.

**Acceptance:** Five local CTest executables passed in optimized GCC C++17 Release mode. Five Clang 17 ASan+UBSan+leak-detection executables passed, including the new memory suite. The memory suite validates packed/default table byte counts, tombstone handling, allocation failure, payload preservation, shrinking/empty release, 23-byte string SSO, heap/inline transfer, string move and shrink, bit-vector reclaim and queue/heap/stack release. Remote clang-format 22.1.5 and GitHub Linux/Windows CMake/FASTBuild are required before declaring integrated acceptance.

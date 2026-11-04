# 21 — SIREngine String Pool v1

**Status:** Six-tier string-pool implementation with native Linux/Windows CMake/GoogleTest integration verified in PR #124. LLVM coverage threshold: 90% per metric (interim).

## Provenance

This port was derived from user-uploaded `nframework.zip` (SHA-256 `b648a15d2420bd5d13d8fd70343c1430a16c85aafed117130602a4350fe042f6`) at `Framework/Core/Containers/NMDStrings.h` and `NMDStrings.cpp`.

Donor file digests:
- Header: `e2bf82757731d79b7f4a5d292939a619c0dc9fc0c15f50488c0430f302d2f148`
- Source: `907f772feebdc107db547b411bae4b78ef6a7636a3cb5a1f1e65051138f7cb63`

## API and ownership

- `SIRStringAtom` is a four-byte process-local identity. Atom values are never persisted and are invalid after pool retirement/reinitialization.
- `SIRStringPool` interns UTF-8 names, maintains one hash index, and resolves cooked ordinal-to-atom identities.
- `SIRStringLease` owns a cache pin or direct-string lease. The pool must outlive every lease and `SIRStringTextRef`; failed `shutdown` is a hard lifetime violation in destructors.
- Memory comes from a supplied SIRMemory-backed `sirContainerAllocator_t` or a caller-owned span. No global allocation fallback.
- Cooked entries are immutable after successful catalog validation. Dynamic names are bounded and append-only until explicit scope retirement.
- SIMD/grouped index and adaptive index are compile-time experimental switches, not a runtime ABI.
- The optional XXH3 hash lookup fast path requires a future explicit pinned `xxHash` dependency; the current default is FNV-1a with full bytewise collision resolution.

## Catalog version reset

The new `SIRS` catalog has magic `SIRS`, version **1**, an explicit 48-byte little-endian header and 8-byte records, with raw and backreference-compressed UTF-8 payloads. It preserves the donor's structural grammar but does **not** accept the prior `NMDS` v2 magic. A SIREngine-native cooked catalog generator must output `SIRS` v1, recalculate payload and corpus hashes, and produce deterministic sorted unique names. Existing NMDS v2 artifacts must be recooked.

## Concurrency

A spinlock serializes string admission; a separate cache lock serializes lazy cooked decoding and eviction. Atom publication is release ordered; readers use acquire semantics. Caller establishes an exclusive boundary for pool shutdown and scope retirement.

## Integration notes

`Source/Core/Strings` is linked into SIRCore through `add_subdirectory(Strings)`. Optional GoogleTest target `core.strings.pool.googletest` integrates in `Tests/Unit/StringPoolTests.cmake` when `SIR_WITH_GOOGLETEST` is enabled. Tests include malformed cooked catalogs, valid/invalid UTF-8, dynamic storage exhaustion, atom/lease lifetimes, scope retirement, concurrent interning, cache eviction, allocation faults and mutated backing-catalog detection.

## Verification state

The original donor port and six-tier implementation have passed the native GitHub Linux and Windows C++17 CMake/GoogleTest builds and Linux Google Benchmark job. The SIREngine pinned GoogleTest target is `core.strings.pool.googletest`. Sanitizer and alternate-index runs also pass in the local Clang 23 test fixture. The full repository-wide Clang AST qualification is separate from this subsystem's compiled test scope.

## Six-tier decoded-string cache (November 2026)

`SIRStringPool::defaultConfig()` now enables `bTieredCache` with **six separately budgeted classes**. The earlier two-class path remains available by setting `bTieredCache=false`; this also preserves existing callers that configure `nShortSlots`, `nLongSlots`, `nShortBytes` and `nLongBytes`.

| Tier | Slot maximum | Bytes per slot | Default initially active |
|---|---:|---:|---:|
| Tiny | 512 | 16 | 16 |
| Small | 256 | 64 | 8 |
| Short | 128 | 128 | 4 |
| Medium | 64 | 256 | 2 |
| Large | 24 | 512 | 1 |
| Massive | 12 | 1,024 | 1 |

The six-tier hard maximum is **996 slots**. `nTierSlots[]`, `nTierBytes[]` and `nInitialTierSlots[]` allow caller-selected profiles; preflight rejects unsorted sizes, maximum sizes below the engine cooked-text limit, overflowing slot totals, or initial allowances exceeding hard caps. When `bAdaptiveBudgets` is false, all configured slots become active at initialization. `getStats().nActiveTierSlots[]` exposes per-tier active limits; the older short/long aggregate stats sum tiers 0–2 / 3–5 in tiered mode.

On a cache miss, select the smallest compatible tier and grow its active allowance (bounded doubling) when a virgin slot is needed. Reuse unpinned slots within that tier. If its existing slots are all pinned, try a larger compatible tier without invalidating any live lease. An overwritten entry has its cooked hot hint cleared before decoding; failed decompression releases the reserved pin without consuming an adaptive admission budget. Pin-safe cache ownership remains serialized through the pool's cache lock. Scope retirement resets active allowances and cache residency; it requires all leases to have been released.

The default hard identity limit remains **8,192 names** with a 16,384-bucket index at 50% target occupancy; active name admission starts at 512 identities. The dynamic UTF-8 arena remains 1 MiB reserved, initially limited to 64 KiB of admitted text. Both grow as demand arrives within their hard maximums. The backing memory allocation still reserves the hard maximum up front. Adaptive budgets control active *usage*, not virtual reservation, page commitment or the lifetime of atom IDs.

## Local performance and verification

A release-mode comparison on a virtualized Linux AMD EPYC host, Clang 23.1.5 `-O3`, measured median acquire/release latency with pre-activated budgets. Across seven randomized trials per layout and workload, the six-tier implementation was 2.94x faster for 64-name uniform access, 2.06x for 256-name uniform access and 1.93x for an 80%-hot/20%-scattered 4,096-name corpus. A full 4,096-name sequential scan improved 1.08x. These are local microbenchmarks, not game runtime performance guarantees.

The expanded compiled regression suite passes in standard, grouped-index and adaptive-index configurations, plus ASan/UBSan. Latest instrumented Clang/LLVM 23.1.5 coverage across `SIRStringPool.cpp` and `.h`: **82/82 functions (100%), 1,251/1,307 lines (95.72%), 677/746 branches (90.75%), and 1,141/1,178 regions (96.86%)**. A source-scoped CMake coverage gate confirms that all four metrics meet the current **90% interim requirement**. Tests use a dedicated friend-only fault-injection fixture for checking defensive branches; the pool's runtime storage layout is unchanged. Linux/Windows native GitHub CI and pinned GoogleTest exercised the prior six-tier revision; this candidate must be rechecked against the new test commit before merging.

## Current acceptance policy

The project-wide interim coverage policy is defined in `Docs/Architecture/11-acceptance-and-validation.md` and enforced against source-scoped LLVM JSON via `Build/CMake/SIRCoverageGate.cmake` (minimum **90% each** for lines, branches, functions and regions). It does not waive native CMake/CTest, sanitizer, compilation or formatting checks. The complete-tree AST gate remains separate.

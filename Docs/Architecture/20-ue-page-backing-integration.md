# Unreal-inspired page backing: implementation checkpoint

Integration: consolidated GRDG/Core infrastructure PR #23; original feature source head `d01d9b1febf9d2893c050c05435f8af0d2caabef`.
Design reference: `SIREngine_UE_OS_Integration_Plan` (November 3, 2026).
This implementation is independently authored for SIREngine, using Unreal's page-cache design only as research.

## Landed in the initial branch

**M0: backing telemetry.** `SIRMemoryManager::getPageBackingStats()` exposes managed
committed bytes (including allocator metadata), actual successful OS commit and decommit
call counts, failed segment commit/decommit calls, duplicate/no-op range requests,
their requested byte counts and commit-budget denials. These are commitment metrics,
**not resident physical memory (RSS)**. The existing `sirMemoryStats_t` remains
ABI-stable.

**M1: opt-in deferred backing release.** Each segment has two manager-owned
bitsets, one for committed pages and one for pages retained pending decommit.
The single manager mutex serializes both bitsets and the global/domain/segment
commit-byte accounting; only `SIRVirtualMemory` performs OS page operations.

- `SIRMemorySegment::bindRetirementOwner(id, generation)`: one-time binding for
  a stable segment owner; both values must be nonzero.
- `retireIdleRange(offset, size, id, generation)`: all pages must already be
  committed and must not already be pending. The caller **must prove at its
  allocator safepoint** that no live allocation, reader, I/O request or other
  pinned reference can touch the range. No syscall occurs during retirement.
- `cancelRetiredRange(offset, size, id, generation)`: the exact requested
  pages must all be pending; reuse retains existing contents with no OS syscall.
- Existing `commit(offset, size)` reacquires overlapping pending pages upon
  successful commit/no-op; existing immediate `decommit(offset, size)` retains
  its invalidating semantics, clearing any overlapping pending marks only
  **after** successful OS decommit.
- `SIRMemoryManager::drainPendingDecommits(maxBytes)`: explicitly called by a
  cold coordinator **after owner safepoints**; drains consecutive pending pages
  in bounded runs. A failed syscall retains pages, data access and the full
  budget charge. Successful runs update the segment, domain and manager
  accounting once and report per-run successes/failures.

**No automatic retirement is enabled.** Existing allocators are unchanged;
they must opt in after their own liveness/epoch contracts have been verified.
This prevents the backing manager from guessing which live pages are safe
to discard. There is no secondary competing committed-page accounting ledger.

## Validation tasks

- Existing `core.memory.manager` now exercises telemetry, owner generation,
  duplicate retirement, budget enforcement while pending, reuse without data loss,
  bounded drain, cancellation and clean immediate decommit.
- Linux `core.platform.decommit-failure` now injects failing `madvise` into a
  pending-page drain and verifies retained bytes, accounting and data accessibility.
- Required before merging: pinned clang-format 22.1.5; Windows/MSVC and
  Linux native CMake tests; sanitizer and memory-manager integration tests.
- Benchmark M0/M1 against the base: mixed 4 KiB/64 KiB/2 MiB page ranges,
  global/domain commitment peaks, syscall counts, p50/p99 latency and fault
  counts. Do not claim a performance gain without these measurements.

## Next integration steps

M2: native allocator-specific retire/reacquire integration with actual object liveness,
pin epochs and bounded warm budgets; then `SIRMemoryPressureCoordinator`
drain orchestration. M3: native Linux PSI and Windows memory pressure
monitoring under Core/Platform, only signaling the coordinator. Native file mapping and GRDG staging now share the consolidated branch. Async I/O and pressure orchestration remain future work.

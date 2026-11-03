# SIREngine allocator acceptance matrix

Source baseline: `feat/memory-framework-allocators-20261103` (PR #12). The native allocator smoke test remains registered as `core.memory.allocators`. When `SIR_WITH_GOOGLETEST=ON`, the exhaustive GoogleTest executable is built and each individual case is registered as a separate CTest test under `core.memory.allocators.*`.

## Coverage

| Category | Allocators and services | Tests |
| --- | --- | --- |
| Linear and two-ended bump | `SIRLinearAllocator`, `SIRHighLowAllocator` | Alignment sweep, invalid sizes, bookmarks/rewinds, exhaustion, reset/decommit |
| Fixed-size pools | `SIRPoolAllocator`, `SIRBitmapAllocator`, `SIRSlabAllocator` | Seeded live-slot trace, non-overlap, byte-pattern integrity, arbitrary free order, invalid/double frees, full capacity and reuse, trim |
| General variable-size | `SIRTLSFAllocator`, `SIRFreeListAllocator`, `SIRBuddyAllocator`, `SIRPageRunAllocator`, `SIRMediumPageAllocator` | Thousands of seeded allocations and frees per backend, mixed alignments/sizes, live-range non-overlap, content integrity, reclamation |
| Transient and epoch | `SIRFrameAllocator`, `SIRJobScratchAllocator`, `SIRVirtualStackAllocator`, `SIRRingAllocator` | Pending fences, stale job token rejection, nested scope unwinding, sequence retirement/backpressure, empty trim |
| Relocatable and evictable | `SIRHandleHeap`, `SIRCacheAllocator` | Pinned stability, generation invalidation, compaction data integrity and admission budget, CLOCK eviction, reclamation respects pins |
| Size classes | `SIRSizeClassAllocator`, `SIRReclaimableSizeClassAllocator` | Every regular and fine class, class boundary mapping, exhaustion/reuse, requested-byte accounting and cold-slab reclaim |
| Concurrent | `SIRCompactBucketAllocator`, `SIRThreadLocalAllocator` | Remote frees, inbox drain, attachment ownership, stale-generation tokens, simultaneous worker seats and teardown |
| Routing | `SIRAllocationRouter`, `SIRGeneralHeap` | Small/medium/large route selection, zeroed and aligned allocations, realloc data preservation, wrong-thread and invalid requests |
| Page authority and pressure | `SIRMemoryManager`, `SIRSharedMemoryPool`, `SIRMemoryColdPageStager`, `SIRMemoryLayoutPlanner`, `SIRMemoryPressureCoordinator` | Commit cap denial, live-byte preservation, no unsafe reclaim, cold-stage range merge/order/capacity, region bounds, layout preflight |

## CI gates

1. The existing Windows and Linux native CI jobs configure `SIR_WITH_GOOGLETEST=ON` and run all CTest cases, including the individually discovered allocator tests.
2. `Source hygiene and formatting` checks the pinned clang-format 22.1.5 and the engine's first-party source rules.
3. The allocator sanitizer workflow runs CTest's allocator suite on Ubuntu with AddressSanitizer and UndefinedBehaviorSanitizer enabled. It fails on sanitizer diagnostics, compilation errors, failed assertions, or unexpected test crashes.
4. `Google Benchmark` is the separate profiling-smoke gate. Profiling output does not replace correctness evidence.

## Reproducibility

The randomized tests use fixed xorshift seeds and deterministic operation counts. A reference ledger checks every live payload for overlap and byte corruption; the test fails at the first observed violation. CTest discovers cases individually, so failures identify the exact class/invariant. The CTest suite uses limited iterations by default to fit the PR matrix; extended soak runs are a separate acceptance tier.

To reproduce the native CI configuration (after initializing pinned GoogleTest):

```sh
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset dev
ctest --preset dev --output-on-failure -R '^core[.]memory[.]allocators'
```

To reproduce the sanitizer configuration on Ubuntu:

```sh
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON -DSIR_ENABLE_ASAN=ON -DSIR_ENABLE_UBSAN=ON
cmake --build --preset dev
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --preset dev --output-on-failure -R '^core[.]memory[.]allocators'
```

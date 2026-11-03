# 18 - Container allocation integration

**Status:** implementation in `feature/core-containers`, synchronized with the current `develop` allocator foundation. The initial integration covers **arbitrary-lifetime** containers through `SIRGeneralHeap`; specialized frame/job/pool adapters are a separate contract with different destruction semantics.

## Verified source contracts

- `sirContainerAllocator_t` already provides callback allocation, deallocation, domain ID, and owner tag. Every dynamic container uses this descriptor and does not fall back to process malloc.
- `SIRGeneralHeap` routes small allocations to `SIRCompactBucketAllocator`, medium allocations to `SIRFreeListAllocator` or `SIRTLSFAllocator`, and large allocations to `SIRBuddyAllocator` or `SIRPageRunAllocator` using three permanently assigned `SIRMemorySegment` instances.
- `SIRMemoryManager` owns page reservation, commit budget, segment/domain attribution, and commit/decommit traces. `SIRGeneralHeap::free` is backed by `tryFree`, which validates the ownership range.
- `SIRAllocationRouter::free` requires a `sirRoutedAllocation_t` containing the original selected backend. The 12-byte record cannot be reconstructed from a general container's pointer alone. A generic adapter for that router must retain a per-allocation route record and its alignment, or use a per-container typed allocator where the route is immutable.

## Integration order and contracts

1. **Synchronize `feature/core-containers` with `develop`:** preserve both parents, keep all memory/allocator tests and container tests registered in CMake. Avoid rewriting shared history.
2. **Unify arbitrary-lifetime containers:** use `SIRContainerHeapAdapter` to bind an initialized `SIRGeneralHeap` to a `sirMemoryDomain_t`; validate that all three real backing segments belong to the requested domain before returning a descriptor.
3. **Test actual backing paths:** exercise small, medium and large container allocations, overflow/rejection, over-alignment, string/hash/vector/page lifetimes, cross-domain failure, backend shutdown and zero live allocations, under the real `SIRMemoryManager`.
4. **Profile:** compare direct `SIRGeneralHeap` calls, the callback adapter, and container operations on the same initialized heap; record steady allocation count, memory-manager domain committed bytes, and CPU timings without conflating requested bytes with OS commit.
5. **Specialize when warranted:** use `SIRReclaimableSizeClassAllocator` or `SIRSlabAllocator` for small fixed-node/page containers after matched measurements. Do not use frame/job epoch allocations as ordinary per-object free backends: those lifetimes are intentionally different.

## Ownership and failure rules

- Construct and initialize the memory manager, segments, backends, `SIRGeneralHeap` and `SIRContainerHeapAdapter` before their containers. Destroy all containers **before** retiring the adapter, heap, backends, and manager.
- Each heap accepted by the adapter must use **one** configured domain across all three backing segments. Multiple domains require separate heap configurations. Owner tags are metadata for the caller and are **not** per-owner budget enforcement.
- The adapter adds no allocation header. A successful allocation is owned by the `SIRGeneralHeap` that produced it, with the domain accounted through that heap's segments. Rejected allocations return null and are converted to `SIR_CONTAINER_OUT_OF_MEMORY` by `SIRRawStorage`.
- The callback free path uses `tryFree` and asserts its success in debug. Caller shutdown must reject live backend allocations. The callback API has no failure-return channel, so freeing through a different allocator is invalid.
- Page commitment after a container's `clear()` may remain retained by a backing allocator; `getFootprint()` records container-requested bytes, whereas manager stats track committed OS pages. A `tryShrinkToFit()` only guarantees a release request, not an immediate OS decommit.

## Research pointers

- EASTL's allocator design uses instance-bound allocator state and explicitly exposes allocator ownership; it also emphasizes fixed-capacity containers and predictable empty-container behavior (EASTL `doc/Design.md`).
- EASTL best practices recommend specialized fixed pools for bounded allocations (`doc/BestPractices.md`).
- GDC's *Solving Memory Issues at Scale: Anvil's Memory Analytics Pipeline* advocates longitudinal allocation/free traces and build-over-build budget comparison; application-level trace collection should use SIREngine's existing `SIRMemoryManager` rather than duplicating global trackers.

## Acceptance gates

- Native Linux and Windows CMake/FASTBuild CI and pinned clang-format 22.1.5.
- Local unit and sanitizer tests for direct callback dispatch, wrong-domain binding, allocation failure, alignment, storage reclamation, and zero leaked live allocations.
- Source-based ownership review and a real benchmark before claiming route performance improvements.

# 04 - Containers and allocation policy

**Status:** SIREngine-owned container implementation in progress on [`feature/core-containers`](https://github.com/SIGAAMDAD/SIREngine/tree/feature/core-containers), PR [#8](https://github.com/SIGAAMDAD/SIREngine/pull/8); the initial SIRGeneralHeap domain adapter is implemented; GRDG and specialized lifetime integrations remain pending.

## Ownership

Portable container code lives under `Source/Core/Containers/`. Every dynamic container receives a caller-owned `sirContainerAllocator_t` with explicit allocation and deallocation callbacks, domain ID and owner tag. A container cannot silently fall back to an untracked process heap. The callback descriptor must outlive every container and every storage allocation it owns; allocator-domain enforcement and full tracing are provided by the NMD integration, not the container headers alone.

Containers use C++17 and global-namespace SIREngine symbols. Fixed-capacity and inline containers retain their actual object-storage cost even when allocating zero bytes externally. Report `sizeof(container)`, live domain bytes, peak committed/resident bytes and allocation calls separately.

## Implemented families

- Contiguous: `SIRArray`, `SIRVector`, `SIRInlineVector`, `SIRStaticVector`.
- Associative: `SIRFlatHashMap`, `SIRFlatHashSet` and `SIRHashMap`/`SIRHashSet` public wrappers.
- Compact: `SIRString`, `SIRBitVector`, `SIRFixedBitset`.
- Indexed/links: `SIRPagedArray`, `SIRIntrusiveList`.
- Adapters: `SIRStack`, `SIRQueue`, `SIRPriorityQueue`, `SIRRingBuffer`.

The current aliases do not provide separate map/set algorithms. The present queues and maps are **single-threaded**; SPSC, MPSC, MPMC and sharded concurrent containers require independent contracts and implementations. The rest of the 38-family plan is future work, not an implied API.

## Imported-library influence

See [15 - Container reference decisions](15-container-reference-decisions.md) for source-by-source decisions, which structures SIREngine intends to adopt as algorithmic patterns, allocation and stability tradeoffs, and provenance. See [16 - Container performance baselines](16-container-performance-baselines.md) for measured values, source revisions, and remaining gates.

Do not conflate benchmark supremacy on a synthetic key sequence with suitability under GRDG workloads. The container library has to enforce domain ownership, handle stability where required, bounded reclamation and an explicitly documented iterator invalidation contract. EASTL remains available through its pinned `ThirdParty/EASTL` dependency for consumers that require it; a SIREngine-owned container is only a substitute after equivalent runtime, correctness and allocation tests pass.

## Acceptance

Every implementation needs positive and negative tests for empty/full, 32-bit size boundaries, alignment, exception/failure rollback, aliasing across growth/rehash, over-aligned objects, move-only values where supported, stale handles, memory reclamation and compiler modes. Hash tables additionally need structured hash-pattern tests; queues need the producer/consumer configuration and teardown tests that match their actual API. ASan/UBSan/TSan tests and clang-format/Clang semantic gates must be tied to the exact commit/configuration. The full SIREngine build and GRDG integration cannot be inferred from standalone container tests.

**Allocator integration:** See [18 - Container allocation integration](18-container-allocator-integration.md) for the GeneralHeap domain binding, ownership constraints, matched profiling plan, and integration acceptance gates.

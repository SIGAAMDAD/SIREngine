# 15 - Container reference decisions: Boost, STL, EASTL and UE5

**Status:** recorded design decisions and work queue, SIREngine C++17 Core container work. **Source measurements:** four-library container benchmark, 2026-11-02, plus PR #8 audit and local optimization trials. Decisions describe algorithmic features to reproduce independently, not third-party implementations already ported into SIREngine.

## Design invariants

1. Use the one tracked SIREngine allocation interface and caller-specified domain. Retain zero-heap fixed variants and explicitly account for inline bytes.
2. Share low-level kernels only where their object lifetime, iterator invalidation, growth and concurrency contracts agree. Preserve separate implementations where layouts conflict.
3. Prefer bounded and compile-time-specialized hot structures where the subsystem has a known maximum. General-purpose variants remain available for unknown capacity.
4. Keep a canonical resource owner and store compact keys/handles in indexes, scheduling structures and GRDG metadata; containers do not create independent resource payload ownership.
5. Use measured per-operation comparisons instead of unconditional claims that an entire vendor library is faster.

## Boost — features to carry forward

| Reference family | SIREngine intent | State / tradeoff |
| --- | --- | --- |
| `boost::unordered_flat_map` / set | Grouped control metadata, SIMD-friendly candidate filtering, high-quality hash avalanche and contiguous payloads | **Planned** advanced flat-hash kernel; grouped probing absent from present `SIRFlatHashMap`. Favorable hashed lookup benchmark 3.38 ns; investigate with a matched SIREngine benchmark. |
| `boost::container::static_vector` and `small_vector` | Inline/fixed storage without per-object heap allocation until overflow; exact capacity and lifetime controls | **Implemented concept** in `SIRStaticVector`/`SIRInlineVector`; benchmark actual object size and overflow. |
| `boost::lockfree::spsc_queue` | Bounded SPSC ring, separated producer/consumer cursors, acquire/release publication | **Planned** `SIRSpscQueue`; tested vendor wall time 8.82 ns/message under one benchmark, independent from engine budget. |
| `boost::intrusive::list` | Caller-owned nodes and constant-time unlink | **Implemented concept** in `SIRIntrusiveList`; link metadata is counted as object memory. |
| `boost::circular_buffer` | Predictable storage, bounded rotation and explicit full semantics | **Implemented concept** in `SIRRingBuffer`; speed remains a comparison target. |
| `boost::container::flat_map` | Cache-efficient sorted vector for read-heavy, relatively static associative data | **Planned** sorted flat map/set. O(log n) search, O(n) insertion. |

## C++ STL — features to carry forward

| Reference family | SIREngine intent | State / tradeoff |
| --- | --- | --- |
| `std::vector` | Trivial contiguous index access, amortized geometric growth, strong reserve/relocation behavior, self-referential emplacement correctness | **Implemented concept** in `SIRVector`; compare 1.5x growth, code generation, capacity retention and failure cases. |
| `std::array` | Zero-allocation compile-time sized data with direct element indexing | **Implemented concept** in `SIRArray`; object consumes its full element capacity. |
| `std::deque` | O(1) indexed segmented storage and cheap insertion/removal at both ends | **Planned** block directory deque; `SIRPagedArray` covers stable appended-page access, not deque semantics. |
| `std::map` / `set` | Ordered balanced tree with stable nodes and logarithmic lookup/insertion/deletion | **Planned** red-black tree map/set, multikey variants as separate contracts. |
| `std::priority_queue` | Binary heap and compact adaptor API | **Implemented concept** in `SIRPriorityQueue`; test actual element-move and comparator costs. |
| `std::string` | Amortized append growth, short-string optimization, correct overlap handling | **Implemented concept** in `SIRString`; preserve byte/UTF-8 and NUL semantics. |
| `std::unordered_map` | Robust generic insert/erase/lookup correctness and differential reference tests | **Used as test oracle**; node stability belongs to a separate `SIRNodeHashMap` when required. |

## EASTL — features to carry forward

| Reference family | SIREngine intent | State / tradeoff |
| --- | --- | --- |
| `eastl::fixed_vector` / `fixed_hash_map` | Capacity as a compile-time contract and no post-initialization heap requirement | **Partially implemented** for vectors; fixed hash map remains **planned**. Measured fixed-hash lookup about 2.13 ns with zero heap at the tested capacity, but roughly 128 KiB of inline table storage. |
| `eastl::segmented_vector` | Segments that grow without moving existing object addresses | **Related implemented concept**: `SIRPagedArray`; exact segmented growth and indexing variants remain **planned**. |
| `eastl::vector_map` | Sorted-vector associative storage and compact read-heavy layout | **Planned** `SIRFlatMap`/`SIRFlatSet`. |
| `eastl::string` | Small-string storage, fast append and custom allocation domains | **Implemented concept** `SIRString`, with a caller-owned allocator descriptor. |
| `eastl::hash_map` | Node/pool allocator customization and hash ownership | **Planned** stable-node/index variant; current `SIRHashMap` wraps flat storage instead. |
| `eastl::ring_buffer` (bonus) | Bounded circular storage as an opt-in header, rather than an undocumented standard container | **Recorded** correctly; the supplied EASTL version includes this header. |
| `eastl::priority_queue` | Efficient heap adapters with comparator policies | **Implemented concept** in `SIRPriorityQueue`. |

## Unreal Engine 5 — features to carry forward

| Reference family | SIREngine intent | State / tradeoff |
| --- | --- | --- |
| `TArray` / `TInlineAllocator` | Inline-capacity option, cheap direct iteration, explicit growth and compact allocation header | **Implemented concepts** in `SIRVector`/`SIRInlineVector`; platform allocator integration remains distinct. |
| `TSparseArray`, `TSetElementId`, `TMap`/`TSet` | Generation-aware sparse slots, indexed identities and hash directory separated from stored values | **Planned** `SIRSlotMap`/`SIRNodeHashMap` and stable GRDG handles; avoid extra resource payload copies. |
| `TChunkedArray`, `TPagedArray` | Stable-address pages plus compact directory | **Implemented concept** in `SIRPagedArray`; separate growable segmented vector remains planned. |
| `TSortedMap` | Ordered flat-map alternative that trades insertion cost for compact lookup layout | **Planned** sorted map/set and differential workloads. |
| `TSpscQueue`, `TMpscQueue`, `TQueue`, `TConsumeAllMpmcQueue` | Distinct producer/consumer contracts, bounded queues when possible, clear node-retention semantics | **Planned** 1P/1C, MP/1C and MP/MC structures; batch-consume semantics remain a different interface. |
| `TStripedMap` | Independent hash shards with explicit reader/writer synchronization | **Planned** concurrent map with guarded access and domain-accounted shards. |

**Provenance and implementation boundary:** UE5 5.8.2 source was inspected for comparative engineering concepts, not imported into `Source/`. UE source is governed by Epic's license; any SIREngine-owned implementation must be original and independently tested. Boost, EASTL and C++ library implementations likewise retain their own license and attribution requirements if source is copied rather than only algorithms studied. No `ThirdParty/` source was modified for these decisions.

## Controlled costs to measure

- CPU median/p95/p99 per operation, cold initialization, bulk destruction and steady-state capacity.
- Allocation calls, requested/live/peak/retained/committed bytes, and `sizeof` for each fixed/inline instance.
- Key distribution: sequential IDs, powers-of-two strides, randomized, mixed structural keys, tombstones and maximum load.
- Iterator/reference stability, insertion exception guarantees, rollback after domain allocation refusal, move-only types and aliasing into storage during growth.
- One-thread and oversubscribed producer/consumer scenarios with CPU time as well as message latency; don't call a queue lock-free until its specific algorithm and semantics are validated.

See [16 - Performance baselines](16-container-performance-baselines.md) for measured numerical evidence, and [04 - Containers](04-containers.md) for current code state.

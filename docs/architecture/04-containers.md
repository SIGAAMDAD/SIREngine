# 04 — EASTL foundation and public alias contract

**Status:** the type aliases in `include/sir/core/SIRContainers.h` are implemented; allocator routing is pending.

SIREngine uses EASTL, pinned to a specific source commit. All SIR-owned interfaces use the following aliases under the `sir::` namespace, allowing future specialization and clearer call sites.

| Public type | Current basis | Use |
| --- | --- | --- |
| `SIRList<T>` | `eastl::vector<T>` | Contiguous dynamic storage |
| `SIRHashMap<K,V>` | `eastl::hash_map<K,V>` | Keyed lookup |
| `SIRString` | `eastl::string` | Owned UTF-8 byte string |
| `SIRArray<T,N>` | `eastl::array<T,N>` | Fixed-extent inline array |
| `SIRFixedList<T,N>` | `eastl::fixed_vector<T,N,false>` | No-overflow fixed-capacity list |
| `SIRHashSet<T>` | `eastl::hash_set<T>` | Membership lookup |
| `SIRDeque<T>` | `eastl::deque<T>` | Double-ended queue |
| `SIRStringView` | `eastl::string_view` | Borrowed string slice |
| `SIRSpan<T,N>` | `eastl::span<T,N>` | Borrowed typed contiguous range |
| `SIROptional<T>` | `eastl::optional<T>` | Optional inline value |
| `SIRPair<A,B>` | `eastl::pair<A,B>` | Pair of values |
| `SIRFixedString<N>` | `eastl::fixed_string<char,N,false>` | Inline string, N includes terminator |

For fixed containers, exceeding capacity is a programming error; default overflow-to-heap is disabled. Any bound read view is invalidated according to its underlying owned container or GRDG epoch.

## Allocation migration

Present aliases deliberately preserve EASTL's default allocator behavior to keep the bootstrap small and testable. The next dependency-closed patch introduces `SIRAllocator` backed by NMDMemory domains, then parameterizes aliases and migrates Godot allocation call sites using verified source ownership and ABI checks. Dynamic allocations must become traceable to NMD before a runtime memory-budget acceptance claim.

Use domain-specific allocator templates or resource handles where appropriate; never make container aliases secretly change allocation semantics without documented ABI/source impacts. Favor hot POD records at or below one cache line, contiguous iteration, explicit loops and offline-precomputed data.

## Tests

The bootstrap includes a C++17 container smoke test. Acceptance for later allocator migration requires allocation/failure instrumentation, memory-map equivalence, iterator invalidation tests and representative cache/performance benchmarks. No claim of lock freedom is implied by a hash container.

# NomadFramework foundation migration and memory expansion

**Source baseline:** `nframework.zip` (`b648a15d2420bd5d13d8fd70343c1430a16c85aafed117130602a4350fe042f6`), supplied 2026-11-02; SIREngine `develop` at `a3f50fec3f9b09331da6fb0f1cefc4ae20001a97`. **Status:** Core/Platform services and the original memory page/segment authority have been ported to the SIRCore target; router expansion, allocation-domain routing and majority-framework cleanup remain planned.

## 1. Source-grounded priorities

| Input implementation | Observed state | Action |
| --- | --- | --- |
| `NMDMemoryManager`, `NMDMemorySegment` | One reservation, page-granular commits, two static domains, coarse domain committed-byte statistics | Preserve reservation/page authority; expand allocation domains and attribution |
| `NMDAllocationRouter` | Selects fixed backing allocators, but does not discover owners and cannot enforce all external allocations | Introduce domain-aware, validated allocation headers/handles and explicit adapters |
| `NMDMemoryPressureCoordinator` / cold-page stager | Domain-pressure and staging mechanisms exist | Bind reclamation to resource/job/GPU/audio safe points |
| `NMDPlatformUtils.cpp` | 1,254-line Windows/POSIX implementation; depends on absent `NMDMappedPtr.h` in supplied archive | Port OS primitives as independent SIREngine interfaces; continue file I/O as a follow-on slice |
| `NMDThread`, `NMDMutex`, `NMDSpinLock` | Native Win32/pthread backends and explicit synchronization | Port without Godot dependencies, validate lifecycle and lock semantics |
| `NMDTypedResourceService` | Owns payloads, budgets, decoding, dependencies and eviction | Separate reusable loader/eviction mechanisms from future GRDG identity/publication owner |
| Service/console/audio/network adapters | Mixed persistent `new[]` allocations, provider-specific policies and shared framework interfaces | Move allocation sites to managed domains before runtime-budget certification |

### Refactoring criterion

Refactor for demonstrable defects, duplicated policy, overlong operations, unnecessary indirection, allocator bypasses, incorrect ownership and build coupling. Preserve tested behavior and ABI where consumers exist. Formatting and naming migrations are separate from semantic changes, and must preserve first-party SIREngine global-namespace, C++17, record and header contracts. The archive's missing `NMDMappedPtr.h`, `NMDIntrusivePtr.h`, and `xxhash.h` are explicit dependency blockers, not stubs to improvise.

## 2. Expanded memory system

### Domain model

Domain IDs are centrally registered during boot, before worker threads start. A minimum built-in set comprises `CORE`, `EDITOR`, `GAMEPLAY`, `NETWORKING`, `RESOURCE_RESIDENT`, `STREAMING`, `RENDERING_CPU`, `PHYSICS`, `AUDIO`, `JOB_SCRATCH`, `FRAME`, and `THIRD_PARTY`; game- and provider-specific subdomains are configured rather than embedded as arbitrary enum changes. Game profiles define independent soft and hard limits, reservation limits, allocator policy and reclaim callbacks. Limits, accounting and source tags are per domain even when multiple suballocators share one reserved address region.

### Ownership and accounting contract

- Reserve the virtual address region once per owning manager; commit/decommit exclusively through the manager. Preserve page-aligned segment boundaries and independent metadata accounting.
- A routed allocation carries domain, allocator ID, owner tag, requested/aligned/committed size, alignment and generation or other verifiable ownership metadata. Free/reallocate validates the owner, backing and lifetime. Zero-length requests, overflow, wrong-domain frees and hard-limit violations have explicit failure results; no untracked fallback heap.
- Track live/requested bytes, allocator capacity, retained free capacity, resident/committed bytes, peak, allocation/free counts, failures and callsite provenance separately. Commit bytes are **not** equal to logical allocation bytes.
- Suballocators may retain pages while free slots exist; their page ownership and trim behavior must be attributable. OS-backed external allocations, file mappings and GPU memory must appear in distinct reports, without double counting.
- Use per-thread counters or bounded event buffers for hot-path metrics, then aggregate at declared safe points. Debug attribution can retain callsite IDs without allocating on the trace hot path.
- Add failure injection to test domain caps and allocation exhaustion. A failed admission must not corrupt allocator state, leave partial live records or violate guarantees for previously published objects.

### Lifetime/reclamation

- `FRAME` resets only after its owning frame fence; `JOB_SCRATCH` resets after all tasks using that epoch complete.
- GRDG owns publication and the lifetime of canonical resources; no retired payload is freed while a reader epoch, GPU fence, physics step or audio callback retains it.
- Allocator shrinking and pressure callbacks run on cold/control paths, with explicit owner order and a bounded reclamation budget per frame.
- Record exact initialization and shutdown order: platform -> reservation -> domain registration -> suballocators -> jobs/providers -> consumers; shutdown reverses dependency ownership after draining outstanding work.

### Implementation waves

1. Port `SIRMemoryManager` and `SIRVirtualMemory` as the memory page/segment authority. Test reserve/commit/rollback/decommit and budget caps.
2. Extend domain registration and page accounting before changing suballocators. Add tagged source reports and negative tests for overflow/invalid domain/wrong owner.
3. Port and compare specific allocators (`Frame`, `JobScratch`, `TLSF`, `Pool`, `Buddy`, recyclable size classes) on authentic traffic traces; retain the fastest correct specialization for each domain. No estimated speedup is accepted as measured.
4. Introduce complete routing adapters for Godot-derived `memalloc`, EASTL and each middleware allocator (FMOD, Jolt, Mono, etc. as supported). Check all return/free paths and memory pressure.
5. Integrate GRDG retirement and platform/provider fence semantics; measure p50/p95/p99 allocation cost, peak committed/resident bytes and retained capacity under import, gameplay, world traversal and shutdown.

## 3. Majority-framework refactor scope

The archive contains **134 framework C++ translation units, approximately 46,254 implementation lines** (excluding nine Godot extension translation units and the EASTL runtime shim). The cleanup inventory covers every framework subsystem; the first program of changes targets the source areas accounting for a majority of framework implementation code.

| Framework subsystem | Translation units | Implementation lines | Specific refactor emphasis |
| --- | ---: | ---: | --- |
| Core (including 31 memory TUs) | 56 | 20,116 | Allocation/ownership, platform boundaries, request/job lifecycle, duplicate wrapper layers |
| Audio | 11 | 5,318 | Eliminate independent heap allocations, clarify provider/bank callback ownership and teardown |
| Steam | 15 | 5,300 | Consolidate SDK admission/ownership and callback/state transitions; isolate generic service API |
| FileSystem | 16 | 3,053 | Reduce path/error handling duplication, define ownership of mapping and asynchronous file handles |
| Network | 11 | 2,637 | Unify packet/session allocations and transport versus game-owned payload boundaries |
| Save | 6 | 2,007 | Shared serialization invariants, durable writes and recovery semantics |
| CVars | 1 | 1,898 | Break large registry logic into operations with testable contracts and bounded storage |
| Input | 4 | 1,721 | Separate device backend, binding logic and replay lifecycle |
| Dialogue | 2 | 1,597 | Retain as game-facing optional provider instead of core dependency |
| Logger | 8 | 1,624 | Centralize sinks and formatting cost; provide bounded diagnostics for allocation hot paths |
| Events and Console | 4 | 983 | Normalize callbacks and registration, avoid needless copies and reentrant lifetime hazards |

The first six rows span **115 of 134 translation units and 38,431 of 46,254 implementation lines**. This is a source-inventory coverage target, *not* a claim that every line needs replacement. Progress is measured in reviewed TU coverage, removed duplication, source-backed defect corrections, allocator compliance, and passing regressions—not arbitrary line churn.

### Source-quality acceptance ledger

- Record each source change as `KEEP`, `SIMPLIFY`, `MERGE`, `SPLIT`, `REPLACE` or `REMOVE`, with the affected symbol, implementation reason, and dependent consumers.
- Identify overly broad managers, redundant delegation, trivial wrapper hierarchies, dynamic polymorphism that adds no required behavior, repeatedly recomputed values, unnecessary heap traffic, duplicated checks and unbounded hot loops. Compare actual implementation with caller contracts before assigning a defect classification.
- Review thread handoff safety, lock order, cancellation, shutdown paths and late callback delivery; retain explicit concurrency and error contracts.
- Preserve stable networking and save formats unless a versioned migration is explicitly approved; preserve engine/editor functionality and existing middleware compatibility.
- Use actual configuration-specific Clang ASTs for semantic refactor coverage and a genuine native object/link build for each dependency-closed batch. If the source is missing an authentic compilation database or required headers, record `BLOCKED_AST` for that portion rather than manufacturing successful coverage.

## 4. Framework cleanup waves

1. **Platform closure:** replace platform macros and monolithic OS utilities with explicit `Core/Platform/` APIs. Port virtual memory, native threads, mutexes, conditions, spin locks, clocks and dynamic library loading. Port native file handles, mapped files and directory operations in the next dependency slice.
2. **Allocation closure:** resolve missing headers; remove duplicate includes and circular header edges; unify checked alignment/page logic; port the memory owner and router using platform interfaces rather than second direct OS backends.
3. **Runtime closure:** remove redundant service wrappers only after call graph inspection. Convert job slot/queue, request, CVars and console allocations to domain-owned storage. Define owner-thread and cancellation invariants for every callback.
4. **I/O and resources:** merge duplicated byte/typed loading logic where representations are equivalent. Keep GRDG as sole canonical resource identity and publication owner. Deduplicate file path policies and use transactional saves.
5. **Providers and gameplay-adjacent utilities:** isolate FMOD, Steam, input, networking, compression, event dispatch, and save serialization behind explicit owner APIs. Remove game-specific state from reusable engine services, preserving wire/save format compatibility.

Each wave obtains real source and compiler commands, covers C-family translation units with configuration-specific Clang AST where a broad semantic refactor is attempted, then runs the affected object/link/test gate. Review high-complexity functions and redundant wrappers with caller evidence; do not mechanically delete interface layers on appearance alone. Maintain a deletion ledger and behavior tests for each removed path.

## 5. Initial platform slice acceptance

Implemented here: the split `SIRVirtualMemory`, `SIRDynamicLibrary`, `SIRPlatformTime` and `SIRPlatformProcess` services with OS-specific Windows/Linux translation units, a backwards-compatible `SIRPlatformUtils` forwarding facade, `SIRThread`, `SIRMutex`, `SIRRecursiveMutex`, `SIRConditionVariable`, `SIRSpinLock`, and ``SIRCore` platform source group. The port supplies a C++17 native test executable. It deliberately does **not** claim the full original file/directory API, platform-independent file watchers or the allocation router and allocator families have been migrated.

Linux gate: compile the native target with GCC and Clang, run the platform test covering valid and invalid page operations, load/symbol/unload and rejected double load, thread completion, condition-variable wait/notify and lock transitions; run Clang ASan/UBSan and per-TU AST parsing. Windows gate: compile with the pinned MSVC/Windows SDK and execute a DLL loading and thread/sync smoke test on Windows. The absence of the exact pinned `clang-format` or Windows toolchain is recorded as NOT_RUN, not PASS.

## 6. Consolidated Core migration (2026-11-02)

The initial central page owner was ported as `SIRMemoryManager` and `SIRMemorySegment` under `Source/Core/Memory/`. Its domain enumerators have been expanded to the thirteen built-in identifiers above while the underlying manager remains a page-reservation/commit authority with domain-level committed-byte limits. Allocation-header attribution, route ownership and suballocator accounting are not yet implemented. SIRCore compiles the platform and memory translation units directly, avoiding a separate system runtime target. The original NomadFramework's remaining classes are *not* part of the linked SIRCore target. Their mechanical `NMD*` to `SIR*` rename is separately staged and requires dependency-aware integration and compilation before publication.

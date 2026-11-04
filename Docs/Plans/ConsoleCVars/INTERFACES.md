# Proposed native interfaces and serialization contracts

**Status:** P0 types and catalog header validator implemented in Source/Core/Registration/SIRRegistrationTypes.h; later interfaces remain design sketches. See [P0 Freeze](P0_FREEZE.md) for binding ABI v1 decisions.
**C++:** global namespace, C++17, `noexcept`, SIR struct/enum names, fixed-width integer types, **at most four parameters per method** and one-line declarations within the 140-column rule. Class groups ordered public/protected/private; constructors/destructor group, operators, getters/setters, ordinary methods.

## 1. Shared types

- `sirStringAtom_t`: 32-bit process/epoch-scoped identity, zero invalid.
- `sirCVarHandle_t { uint32_t nSlot; uint32_t nGeneration; }`: 8-byte value, zero slot invalid.
- `sirCommandHandle_t { uint32_t nSlot; uint32_t nGeneration; }`: same invalidation semantics.
- `sirCVarType_t`: bool, i32/u32/f32, i64/u64/f64, enumeration, UTF-8 string; vector/path extension unresolved.
- `sirCVarScalar_t`: tagged fixed-width scalar payload (never untagged pointer alias).
- `sirCVarOrigin_t`: `eSource, nCapabilities, nIssuerId, nSourceRevision` (stable origin descriptor).
- `sirCVarWrite_t`: handle, typed value, origin and optional expected registry revision.
- `sirCVarTransaction_t`: bounded staged writes + source overrides, owned by CVar registry; distinct prepare/commit/discard states.
- `sirCommandSubmission_t`: owned input buffer lease/offset, origin, request identity and diagnostics sink reference.
- `sirCVarChangedEvent_t`: handle, atom, type, old/new stable value snapshot or owned string leases, origin, registry revision and tick.
- `sirCommandArguments_t`: callback-duration-only immutable parsed views + typed validated argument array.
- `sirCommandContext_t`: origin, request ID, console output sink, module execution lease and cancellation state.

Public type/layout sizes and field offset assertions are due at P0/P3; do not serialize native structs.

## 2. Result status and writer rules

`sirConsoleResult_t` and `sirCVarResult_t` cover `OK, NO_CHANGE, NOT_FOUND, INVALID_ARGUMENT, NOT_INITIALIZED, TYPE_MISMATCH, STALE_HANDLE, BAD_SCHEMA, WRONG_THREAD, DENIED, QUEUE_FULL, CAPACITY_EXCEEDED, OUT_OF_MEMORY, CANCELLED, MODULE_UNLOADED, CALLBACK_FAILED, SHUTTING_DOWN`.

Read functions are idempotent, side-effect-free, and never intern transient strings. Cold registration writes and typed value mutation are owner-thread operations. Cross-thread submit only enqueues; it never executes callbacks. Failed mutations preserve the previously published effective revision. Retry of a completed request ID returns or references its completion when retained in bounded history; old IDs outside retention are explicitly treated as new requests or rejected by declared policy (decision required).

## 3. Callable contracts

The following is an API skeleton, **not** code presently in the repository:

| Owner | Callable (all `noexcept`) | Effect / failure / caller |
|---|---|---|
| `SIRStringPool` | `find(sirStringView_t, sirStringAtom_t*) const` | Read-only; NOT_FOUND or bad encoding. |
| `SIRStringPool` | `intern(sirStringView_t, uint32_t ownerTag, sirStringAtom_t*)` | Cold writer; bounded, collision-safe; no implicit malloc. |
| `SIRRegistrationCatalog` | `load(sirCatalogBytes_t, const sirContainerAllocator_t&, sirCatalogResult_t*)` | Stage/validate schema+hash, atom remap, then atomic descriptor publication. |
| `SIRCVarRegistry` | `find(sirStringAtom_t, sirCVarHandle_t*) const` | Read-only, checked epoch; does not allocate. |
| `SIRCVarRegistry` | `getScalar(sirCVarHandle_t, sirCVarScalar_t*) const` | Typed scalar read; stale/type errors. |
| `SIRCVarRegistry` | `readString(sirCVarHandle_t, sirCVarStringLease_t*) const` | Read lease pins current owned bytes. |
| `SIRCVarRegistry` | `setScalar(sirCVarHandle_t, sirCVarScalar_t, sirCVarOrigin_t, sirCVarWriteReceipt_t*)` | Single-value transaction; owner thread; origin+priority checks; no partial mutation. |
| `SIRCVarRegistry` | `prepareBatch(sirCVarBatchView_t, sirCVarOrigin_t, sirCVarTransaction_t*)` | Reserves values, event credit, storage and validates every entry; no publication. |
| `SIRCVarRegistry` | `commitBatch(sirCVarTransaction_t*, uint64_t tick, sirCVarWriteReceipt_t*)` | Exactly once; publish coherent revision and ordered mandatory events or fail without effects. |
| `SIRCVarRegistry` | `removeOverride(sirCVarHandle_t, sirCVarSourceKey_t, sirCVarWriteReceipt_t*)` | Re-evaluate lower priority value. |
| `SIRCVarRegistry` | `applyLatched(sirCVarApplyPhase_t, uint64_t tick, sirCVarWriteReceipt_t*)` | Phase-locked pending→effective change. |
| `SIRCommandRegistry` | `bind(sirBindingId_t, sirCommandCallback_t, sirModuleLease_t)` | Cold bind; rejects missing schema/owner and duplicates. |
| `SIRCommandRegistry` | `registerDynamic(sirCommandDescriptorView_t, sirCommandCallback_t, sirModuleLease_t, sirCommandHandle_t*)` | Cold safe-point publish; no implicit global name override. |
| `SIRCommandRegistry` | `unregister(sirCommandHandle_t)` | Mark unavailable, cancel/drain queued invocation, then release module lease. |
| `SIRConsoleService` | `submit(sirStringView_t, sirConsoleOrigin_t, uint64_t requestId)` | Bounded enqueue with owned byte copy/lease; queue full rejects without side effects. |
| `SIRConsoleService` | `pump(uint64_t tick, uint32_t maxCommands, sirConsolePumpReport_t*)` | Owner thread; bounded parse/execute, no reentrant inline execution. |
| `SIRConsoleService` | `complete(sirStringView_t prefix, sirCompletionBuffer_t*) const` | Bounded sorted suggestions; read-only; no callback execution. |

All callback pointers are resolved at runtime by binding ID; the callback type is a `noexcept` native function pointer of three parameters (context, invocation context, argument view). The callback must not retain parser scratch. `sirModuleLease_t` pins executable module code through completion. No exception, allocation fallback or shutdown-after-use is allowed.

## 4. Binary registration schema (proposed, exact byte ABI deferred to P0)

Header must contain `magic, schemaVersion, headerBytes, byteOrder, generatorRevision, recordCounts, recordOffsets, recordStrides, stringCatalogHash, descriptorHash, checksum`. Record groups contain strings, CVars, commands and scalability presets. Integers are explicitly little-endian, offsets are checked 64-bit positions from blob start, records individually aligned by declared rule. Reject `count * stride` overflow, sections outside blob, misalignment, overlapping sections, noncanonical names, duplicate command/CVar names, duplicate stable binding IDs, invalid flag combinations, unknown required schema, wrong string catalog hash or unbound required callbacks. Baked names use catalog ordinals, never runtime atoms. Stable schema IDs must survive catalog reorder.

## 5. Name, source and transaction invariants

- A key resolves uniquely across command and CVar registries. Built-in reserved keys cannot be shadowed by module registration.
- Dynamic re-register of a different type or owner under the same effective key rejects.
- Stale handle after module unload/slot reuse rejects; identical slot value with a new generation is a different identity.
- Source history is cold; effective scalar remains in hot bank. Equal-priority tie-break and full precedence table are **OPEN**.
- Every failed batch has zero value/revision/event effects; a batch that changes only lower-priority history may have no effective-change event but records a separate origin-history revision.
- Event queue credit must be guaranteed before committing. Existing `SIRDeferredEventChannel` does not expose reservations. P3 must provide a documented credit guarantee or a CVar-owned bounded staging publication channel, not a blind `enqueue` after commit.
- A mutable string's old bytes cannot be freed until reader and event leases release; repeated replace must reclaim live bytes.
- Reader publications are coherent per committed revision; CVar consumers must not directly mutate renderer, physics or audio provider internals.
- Config parse errors produce zero command effects. `exec` is an explicit authorized consumer of parsed statements.

## 6. Deferred owner decisions

1. Priority ranking across device profile, project/user archives, command line, editor preview and server authority.
2. Whether CVar types include vectors/colors/paths in P3 or remain typed scalar+string initially.
3. Exact namespace grammar and case-fold admission policy.
4. Which mandatory event-credit approach is implemented.
5. Source format for user archives (existing writer must be implemented first).
6. Apply phases for LATCHED and permission rules across shipping/editor/development builds.

Freeze these as versioned reviewed contracts before dependent source implementation; proposal text is not proof of owner approval.

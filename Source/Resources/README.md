# GRDG resource runtime

This directory now contains the first native, executable GRDG ownership slice:

- Fixed-capacity, generation-safe resource registry and canonical content keys.
- Single-owner writer, immutable published records, read scopes and conservative
  quiescent-state reclamation; read-side atomic operations do not acquire locks.
- Explicit caller-supplied allocator callbacks; all metadata, staging and payload
  allocations are accounted for and budget-checked. SIRMemoryManager integration is pending.
- Borrowed views across resource kinds, staging-to-resident ownership transfer,
  byte-verified chunk deduplication and dependency/cycle validation.
- Provider pins keeping retired revisions alive until consumers release them.

The public declarations reside under `Source/Resources/`. All GRDG types are
in the global scope: SIREngine's resource implementation declares no namespaces.

**Unimplemented:** backend-native derived-resource registry, format-specific
image/mesh/audio decoders, streaming I/O and integration into the Godot-derived
engine/editor. Specialized view methods remain interface declarations until
corresponding cooked schemas and subsystem adapters are established.

See [resource interfaces and graph](../../Docs/Architecture/19-grdg-resource-interfaces.md)
and `Tests/Unit/SIRResourceGraphTests.cpp`.

The optional `SIRResourceMemoryAdapter` binds resource graph allocation callbacks
onto actual `SIRGeneralHeap` instances after verifying each heap's memory domain.
Metadata and streaming allocations can use separate tracked domains. The adapter
and its bound heaps must outlive every graph borrowing the callback context.
The native `resources.memory.domains` test exercises publication, provider pins,
retirement and rejection of an unbound domain against real Core heaps.

Derived provider representations are registered through SIRResourceDerivedRegistry.
The registry stores metadata in a supplied SIR resource allocator domain, pins
the exact canonical source revision, rejects duplicate provider-native tokens,
and accounts for device, external and independent CPU allocation bytes without
recharging canonical GRDG backing. Retirement is deferred until each provider
reports a monotonic completion fence AND its release callback confirms native
deallocation. Failed releases are retried without invalidating handles or
dropping accounted bytes. Providers and the registry must shut down before the
owning resource graph; the registry is single-writer and bounds all metadata.

Writers can call `publishExpected(publication, expectedRevision, handle)` to
reject outdated asynchronous completions before staging is consumed or any
published record changes. Expected revision 0 means absent or requested;
a mismatch returns `SIR_RESOURCE_RESULT_STALE_REVISION`. This is a
single-writer version-admission check, not multi-writer synchronization.

A bounded writer-owned reverse dependency index now maps stable target keys
(including missing optional/streamed targets) to the resources that depend on
them. Publications and retirement update the index transactionally, rejecting
insufficient edge capacity before consuming staging. Each change propagates a
monotonic invalidation sequence through all transitive dependents. Writers
can call `collectDependents` to enumerate direct consumers without scanning the
entire registry or `getInvalidationSequence` to inspect a live resource's
last upstream invalidation. The invalidation sequence is cleared when that
resource successfully republishes; the resource revision still identifies the
replacement. This first index is writer-owned; worker-to-writer notifications
and a bounded multi-consumer change journal remain separate future work.

The bounded invalidation journal stores one ordered notification per affected
resource, each with a unique journal cursor, dependent generation-safe handle,
originating change key and batch sequence. `pollInvalidations` supports partial
drains without allocation. A consumer that falls beyond retained journal
capacity receives `SIR_RESOURCE_RESULT_JOURNAL_OVERFLOW` and must rebuild
its current dependency state before resuming at the returned latest cursor.
The journal and reverse index are single-writer; cross-thread delivery is a
separate scheduler contract.

The canonical read-side `find` now probes a preallocated atomic open-addressed
index. Each live cell references an immutable published record; deletion leaves
a tombstone so future probes cannot miss colliding identities. The writer
updates cells on publication/retirement without moving other live entries.
Read scopes keep old records alive until quiescence, so concurrent readers
can safely inspect a cell during mutation without acquiring a shared lock.
The existing `SIRFlatHashMap` remains the private single-writer index.
This is a bounded key-lookup implementation, not an epoch reclamation scheme.

Per-read-scope epoch announcements now replace global all-reader quiescence
as the collection criterion. Read scopes claim one fixed, metadata-domain
allocated slot; the writer stamps retired immutable records with their
publication epoch and collects each retired record once all remaining readers
are newer than that record, subject to provider pins. A reader entering while
the writer advances an epoch retries its announcement; an entering/reserved
slot conservatively blocks collection. The configured capacity is
`nMaxReaders` (zero defaults to 64); excess scopes report inactive. This
permits reclamation of old revisions while unrelated newer readers remain
active. This does not change the immutable views' scope-lifetime contract.

The writer can now compile a deterministic, dependency-first resource plan
with `compileDependencyOrder(root, entries, capacity, required)`. The compiler
uses preallocated DFS state and stack arrays, has no runtime allocation or
recursion, includes loaded optional/streamed edges, skips absent optional
edges, and fails if required dependencies are missing. Output records capture
generation-safe handles, the current publication revision, and resource state.
A caller can query required output capacity without modifying its array; an
undersized caller buffer remains unchanged. This is the resource DAG compiler
used to prepare work for the separate scheduler; it does not execute jobs or
perform backend scheduling.

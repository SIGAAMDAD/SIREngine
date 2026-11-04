# Streaming foundation — source integration ledger

**Baseline:** `develop` commit `be151c5ad3e98ea6fa6bdbfa87d07a83dc7e6546`.
**Branch:** `feature/streaming-foundation`.
**Status:** demand coordination, World Partition, epoch reconciliation, provider completion bridge and conditional GRDG file-loader adapter implemented; live GRDG build and provider backends pending.

Source design authorities: `SIR-WORLD-ARCH-001`, `SIR-IO-ARCH-001`,
`SIR-IO-IMPL-001`, `GRDG-003` in the shared Nomad Drive. This branch introduces
only the interface owner for request intent and stage tracking.

## Lifetime and ownership

- Coordinator caller controls the lifetime of aligned storage, supplied through
  `SIRMemoryManager` / STREAMING in production; `shutdown` refuses live demand.
- Consumers retain generation-checked demand tickets, not borrowed disk buffers
  or GRDG resource handles. A released slot invalidates every old ticket.
- Cancellation is a flag until actual I/O, GPU, Jolt or other provider work is
  quiescent. Acknowledgement of resource publication is not inferred from
  request submission.
- The coordinator is single-thread-affine; jobs and I/O completion workers must
  marshal operations to the streaming owner. GRDG remains the only publisher.
- One demand per (consumer ID, consumer generation, logical unit) is admitted. Independent
  consumers can request the same logical unit; GRDG performs physical dedup.
- Manifest generation and request epoch are carried in the demand for upstream
  cancellation/version checks. Their binary serialization is not frozen here.

## Still absent

| Milestone | Current state | Dependency |
| --- | --- | --- |
| Native demand coordinator | Compiled lifecycle shell, ticket metadata refresh and unit tests | No GRDG dependency |
| Demand epoch reconciliation | Implemented complete-set validation, stable tickets, pending cancellation, deferred resubmission and integration tests | GRDG completion/retirement adapter |
| Provider lifecycle bridge | Implemented bounded prepare/activate/cancel/retire dispatch and generation-safe owner-thread acknowledgements | Real canonical GRDG mapping and native provider backends |
| GRDG adapter | Concrete opt-in adapter for ResourceGraph, ResourceFileLoad, Core Jobs and provider pins; local fake-backend tests pass | Build/run with PR #23's live Resources integration |
| SIRPack `.sirm/.sirp` reader/cooker | Not implemented | Finalize package ABI |
| Core I/O native async | Not implemented | IOCP/io_uring contracts |
| World Partition/cells/layers | Native sorted cell directory, spatial demand generation, layer filtering and source merge implemented; activation pending | Cooked package/StateStore integration |
| Predictive dispatch | Not implemented | World immutable views and request epochs |
| Entity cooling/waking | Not implemented | Script state codec, WorldState |
| Terrain, VT, HLOD provider callbacks | Not implemented | Terrain port, renderer, GRDG leases |
| Streaming traces/benchmarks | Not implemented | Real provider loads and route captures |

**Initial acceptance:** source compiles as a `SIR::Streaming` library; tests
cover valid/invalid state transitions, staging cancellation, stale ticket reuse,
capacity exhaustion, prepared-only demand and shutdown with live resources.
Full SIREngine CI and native provider hardware benchmarks are separate gates.

## World Partition implementation — next source slice

`Source/World/Partition/SIRWorldPartition` now validates and copies a sorted
cell directory into caller-supplied storage, checks unique cell IDs using an
in-place heap-sorted index, and generates one partition-owned streaming demand
per selected cell. Selection covers source radius, optional vertical strata,
layer eligibility, always-resident cells, overlapping priority/target/deadline
merging and generation-tagged request epochs. Demand output is atomic under
capacity failure. Clang 23 syntax, bounded CMake harness tests and Clang 17
ASan/UBSan were executed; complete engine CI is still pending.

No GRDG/IO publications, runtime actor allocation or provider activation are claimed by the World Partition slice. The cooker must emit the
manifest directory, and the coordinator must reconcile request epochs and
resource provider completions before gameplay can activate loaded cells.

## Epoch reconciliation — third native slice

`SIRStreamingDemandReconciler` consumes the complete per-epoch demand set
produced by `SIRWorldPartition::gather`. It requires a single owner thread,
caller-provided storage from the streaming memory domain and monotonically
increasing, nonzero request epochs. For each unit, it preserves valid tickets,
updates target/priority/deadline in place when safe, cancels disappeared units,
and refuses to treat cancellation as physical resource reclamation. Already
active providers must retire before a PREPARED downgrade can be re-submitted.

The reconciler returns `nDeferred` for blocked re-entry or occupied capacity
and retries on later epochs. A tombstoned ticket is pruned only after the
coordinator reports it stale following provider-owned retirement; no GRDG
publication is simulated. C++17 CMake harness tests include spatial traversal,
reconciliation, invalid-set atomicity, stable ticket retention, promotion,
demotion, cancellation, and a full-capacity pending-provider case. Linux
Clang 17 ASan/UBSan and libclang 23 structural checks passed for the local
candidate. Repository-pinned clang-format 22.1.5 and full project CI remain
outstanding acceptance gates.

## Provider lifecycle dispatch — fourth native slice

`Source/Streaming/SIRStreamingProviderBridge` consumes the coordinator's
bounded slot enumeration and invokes caller-supplied request callbacks.
Acceptance starts work; it does not publish a resource or certify a provider.
Completion acknowledgements must be marshalled onto the streaming owner thread
and occur only after the canonical GRDG writer and the affected provider have
actually completed their relevant work. The bridge retains a generation-tagged
slot until the retirement callback has completed and `acknowledgeRetired`
explicitly releases the coordinator ticket. The bridge does not cache or own
GRDG handles, cooked bytes, GPU allocations, physics bodies or audio banks.

A PREPARED-to-FAILED transition supports activation failures. Downgrades to
PREPARED during outstanding activation are converted into retirement, allowing
the reconciler to request a fresh PREPARED ticket on a later epoch. Stale
completions are rejected after ticket reuse; external ticket bypass fails closed
instead of clearing an in-flight provider slot.

A native CMake harness builds the coordinator, World Partition, reconciler and
provider bridge with Clang 17 and Clang 23. Four test executables pass, including
an integrated moving-source control-plane test. Clang 17 ASan/UBSan tests pass.
The repository-pinned clang-format 22.1.5, full engine build, platform CI, and
live GRDG/IO/provider tests are still outstanding.

## GRDG file-loader integration — fifth native slice

`Source/Streaming/SIRStreamingGRDGAdapter` is the dependency-gated native
resource preparation adapter. It accepts a caller-owned, sorted in-memory
binding directory mapping logical streamed units to cooked resource keys,
file intervals and declared chunk/schema identities. It uses the actual
`SIRResourceFileLoad` request/poll/cancel API and `SIRResourceGraphWriter`
for authoritative staging/publication. Multiple consumers of the same key
wait for one physical in-flight load and subsequently pin the single published
resource individually; no second resource cache or allocator is created.

Preparation checks the published schema, format, chunk identity and declared
content key, then retains a canonical GRDG provider pin. Pinning validates
identity metadata; cryptographic verification of raw file bytes belongs to
SIRPack ingestion/codec validation and is not implemented here. The adapter
passes both GRDG handle and published revision to the backend, monitors
prepared/active revisions, and cancels stale tickets so the backend can retire
safely. Cancellation of an in-flight file read waits for Core Jobs completion
before issuing a final failure/retirement acknowledgment. Physical GRDG
retirement remains in the resource owner; per-consumer retirement only releases
the matching provider pin after backend acknowledgment.

The `SIR::StreamingGRDG` CMake target and its real-file integration test are
built only when `SIR::Resources` exists. Because PR #23 remains an unmerged
draft, Clang 17/23, ASan/UBSan and five focused local harness tests used an
API-matched fake GRDG/file-loader service for this slice. The actual PR #23
library and Windows/Linux production CI were not compiled or executed here;
the native integration test is registered to run when the dependency lands.
The repository-pinned clang-format 22.1.5 gate is still outstanding; source
was formatted and checked with clang-format 23.1.5.

## Canonical alias validation and bounded physical I/O — sixth slice

The optional GRDG adapter now rejects multiple logical binding entries that
name the same canonical `SIRResourceKey` while disagreeing about the source
path, file span, content key, schema, format, alignment, dependency edges,
chunk identity, transfer mode or expected revision. This is a cold mount-time
validation pass; compatible aliases still share the GRDG resource and each
consumer maintains an independent provider pin.

The new `nMaxConcurrentLoads` configuration bounds physical in-flight
`SIRResourceFileLoad` operations. Zero defaults to adapter capacity. The
adapter releases the load budget only when the Core Jobs transfer reaches a
terminal state, including cancelled requests. The per-pump stats report both
current active loads and configured maximum, independent of demand tickets.

The local Clang 17/23 CMake harness and Clang 17 ASan/UBSan complete five
suites with negative alias-directory cases and a three-unit single-load
budget progression. PR #23's real native GRDG/Core Jobs source, full engine
CI and Windows provider runtime remain outstanding checks. The repository
formatter now explicitly binds reference ampersands to types; the source was
formatted using clang-format 23.1.5, while the pinned 22.1.5 gate remains
unverified.

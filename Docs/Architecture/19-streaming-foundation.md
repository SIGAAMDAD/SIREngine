# Streaming foundation — source integration ledger

**Baseline:** `develop` commit `be151c5ad3e98ea6fa6bdbfa87d07a83dc7e6546`.
**Branch:** `feature/streaming-foundation`.
**Status:** demand coordinator, spatial World Partition and epoch reconciliation implemented; no end-to-end asset I/O or world activation.

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
| GRDG adapter | Not implemented | Integrate PR #23 contracts |
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

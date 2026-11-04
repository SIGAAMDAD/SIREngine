# Streaming foundation — source integration ledger

**Baseline:** `develop` commit `be151c5ad3e98ea6fa6bdbfa87d07a83dc7e6546`.
**Branch:** `feature/streaming-foundation`.
**Status:** first implementation slice; not an end-to-end world streaming system.

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
| Native demand coordinator | Compiled shell + unit tests | No GRDG dependency |
| GRDG adapter | Not implemented | Integrate PR #23 contracts |
| SIRPack `.sirm/.sirp` reader/cooker | Not implemented | Finalize package ABI |
| Core I/O native async | Not implemented | IOCP/io_uring contracts |
| World Partition/cells/layers | Not implemented | Cooker manifest, StateStore schema |
| Predictive dispatch | Not implemented | World immutable views and request epochs |
| Entity cooling/waking | Not implemented | Script state codec, WorldState |
| Terrain, VT, HLOD provider callbacks | Not implemented | Terrain port, renderer, GRDG leases |
| Streaming traces/benchmarks | Not implemented | Real provider loads and route captures |

**Initial acceptance:** source compiles as a `SIR::Streaming` library; tests
cover valid/invalid state transitions, staging cancellation, stale ticket reuse,
capacity exhaustion, prepared-only demand and shutdown with live resources.
Full SIREngine CI and native provider hardware benchmarks are separate gates.

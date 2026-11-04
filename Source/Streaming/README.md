# Native Streaming Foundation

**Scope:** engine-level demand and activation intent only. Physical resource
identity, immutable payloads, dependencies, deduplication, reservations and
retirement are owned by GRDG. Core I/O owns file handles, native async reads,
and completion transfer. Core Jobs owns scheduling and worker/provider fences.

## Compiled foundation in this branch

- `SIRStreamingCoordinator` is a single-owner, bounded demand ledger. Callers
  provision aligned fixed storage from the SIRMemory STREAMING domain; no
  allocation or OS I/O happens in the coordinator.
- `sirStreamingUnitKey_t` is a **manifest-local demand unit**, not a new GRDG
  resource key. Version fields let downstream adapters reject obsolete manifest generations.
- Every consumer has an independent ticket. Physical sharing/dedup happens in
  GRDG, not by merging or releasing another consumer's request.
- `QUEUED -> PREPARING -> PREPARED -> ACTIVE -> RETIRING -> EMPTY` is the
  success path. `PREPARING -> FAILED -> RETIRING -> EMPTY` is the failure path.
  A `PREPARED` target stops before `ACTIVE`.
- Cancellation of queued work frees its ticket; an in-flight or provider-owned
  operation stays pinned until completion and retirement acknowledgements.
  The engine must drain outstanding tickets before `shutdown` succeeds.
- `submit`, `updateDemand`, `advance`, `cancel`, and `query` are owner-thread operations only.
  Core Jobs will carry cross-thread producer messages when integrated.

- `SIRStreamingDemandReconciler` consumes complete World Partition demand epochs,
  preserves stable tickets, refreshes priorities/deadlines and target states,
  cancels removed cells, and defers provider-pinned re-entry until retirement.
  It does not advance physical provider or GRDG resource state.
- Demand-set validation rejects stale epochs, duplicate keys, invalid ownership
  and over-capacity snapshots before modifying coordinator tickets.

- `SIRStreamingProviderBridge` now polls coordinator tickets in bounded,
  round-robin owner-thread slices, requests preparation/activation/retirement
  through explicit provider callbacks, and accepts generation-checked
  acknowledgements only after real provider completions. Preparation and
  activation are not assumed successful from dispatch acceptance.
- Cancellation during preparation and activation, activation failure, and
  downgraded demand all retain outstanding provider ownership until retirement
  is explicitly acknowledged. Forced external ticket recycling cannot free
  a provider-pinned bridge slot.

- `SIRStreamingGRDGAdapter` binds sorted manifest-local units to the **real**
  `SIRResourceFileLoad`, GRDG writer, Core Jobs and provider-pinning APIs from
  the independent Resources integration. Concurrent consumers share a canonical
  published resource; the adapter holds one provider pin per live ticket.
  Successful preparation is acknowledged only after a matching published
  schema/chunk and a live provider pin have been verified.
- The adapter rejects obsolete resource revisions, invalid directories and
  overlapping caller-owned storage. Pending I/O cancellation is drained before
  provider retirement. Resource handles are paired with immutable revisions;
  activation and retirement still require the actual backend's safe-point ack.
- The `SIR::StreamingGRDG` target and its file-load test are intentionally
  conditional on `SIR::Resources`. This branch contains concrete integration
  code, but it does not import or claim integration of the unmerged PR #23.

## Module boundaries for the complete system

| Layer | Planned owner | Contract / next component |
| --- | --- | --- |
| State and immutable reader views | `Source/World/State` | `SIRStateStore`, `SIRImmutableViewService` |
| World cells, layers, streaming sources | `Source/World/Partition` | `SIRWorldPartition` directory/demand generator implemented; layer policy and provider activation pending |
| Trajectory + offline prediction | `Source/Prediction` | `SIRPredictionSystem`, versioned demand epochs |
| Entity FULL/REDUCED/COARSE/DORMANT | `Source/World/Entities` | `SIRWorldEntityResidency`, durable event cursors |
| Pack manifests + compression | `Tools/SceneCooker` and `Source/Resources` | `.sirm/.sirp`, deterministic cooker and validated reader |
| Request coordination, dedup, publication | `Source/Resources` | typed bridge to PR #23's GRDG writer, file-load and provider pin APIs implemented; integration build pending |
| Native read/completion | `Source/Core/IO`, platform backends | Windows IOCP, Linux io_uring/pread fallback |
| Terrain-specific residency | `Source/Terrain` | region pages, VT, HLOD, clipmap feedback |
| Provider activation | Renderer, Jolt, navigation, scripting, FMOD | GRDG preparation and pinning adapter implemented; native backend activation/retirement adapters pending |
| Diagnostics | Streaming + Core telemetry | load/eviction traces, load-to-active and p99 hitches |

## Integration sequence

1. Merge the standalone `SIR::Streaming` target and execute the headless
   coordinator lifecycle test on Windows and Linux.
2. Freeze manifest identity and GRDG writer/staging contracts against the live
   resources integration; adapt demand dispatch to the canonical coordinator.
3. Implement SIRPack reader/cooker and native asynchronous I/O. Produce a
   packed fixture and verify request -> read -> decode -> GRDG publication.
4. Implement World Partition, streaming sources, prediction, entity cooling and
   Terrain3D residency as **separate native owners**. No editor node runtime.
5. Bind renderer/physics/audio/navigation provider-ready acknowledgements;
   test teleport, stale generations, cancellations and safe retirement.
6. Measure budgets, p50/p95/p99 load-to-active latency and frame-time hitches.

**Current limitation:** the optional GRDG adapter compiles against its declared API and is tested with a local mock, but the branch cannot link/run PR #23's real loader until it is integrated. SIRPack cooking, native kernel-async I/O and world activation are not implemented. It establishes the
buildable, tested demand/state authority and leaves data-plane ownership intact.

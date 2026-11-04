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
- `submit`, `advance`, `cancel`, and `query` are owner-thread operations only.
  Core Jobs will carry cross-thread producer messages when integrated.

## Module boundaries for the complete system

| Layer | Planned owner | Contract / next component |
| --- | --- | --- |
| State and immutable reader views | `Source/World/State` | `SIRStateStore`, `SIRImmutableViewService` |
| World cells, layers, streaming sources | `Source/World/Partition` | `SIRWorldPartition`, `SIRWorldCellDirectory`, `SIRWorldLayerManager` |
| Trajectory + offline prediction | `Source/Prediction` | `SIRPredictionSystem`, versioned demand epochs |
| Entity FULL/REDUCED/COARSE/DORMANT | `Source/World/Entities` | `SIRWorldEntityResidency`, durable event cursors |
| Pack manifests + compression | `Tools/SceneCooker` and `Source/Resources` | `.sirm/.sirp`, deterministic cooker and validated reader |
| Request coordination, dedup, publication | `Source/Resources` | GRDG writer and native streaming coordinator (PR #23 foundation) |
| Native read/completion | `Source/Core/IO`, platform backends | Windows IOCP, Linux io_uring/pread fallback |
| Terrain-specific residency | `Source/Terrain` | region pages, VT, HLOD, clipmap feedback |
| Provider activation | Renderer, Jolt, navigation, scripting, FMOD | provider-safe fences, callbacks, version guards |
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

**Current limitation:** this branch does not stream bytes, cook packages,
activate worlds or claim integration with unmerged PR #23. It establishes the
buildable, tested demand/state authority and leaves data-plane ownership intact.

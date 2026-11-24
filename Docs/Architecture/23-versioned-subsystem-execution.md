# Versioned subsystem execution foundation

**Source baseline:** `SIREngine_GRDG_DedicatedSync_Source.zip` (local source archive, no Git metadata).

**Scope:** Core tasking integration, timeline, typed command ingress, immutable state projection, and optional scheduled application updates. This is an executable foundation, not a claim that source-absent physics and networking backends have been implemented.

## Execution model

The engine uses **dependency-driven, multi-rate publication** rather than a mandatory all-thread end-of-frame rendezvous.

- The **main/application owner** determines frame and simulation tick boundaries. Its input snapshot is published before scheduled variable updates.
- **Physics** uses fixed-step commands with explicit target ticks. Its registered safe-point executor consumes the commands, advances authoritative physics, and publishes a compact immutable result projection.
- **Rendering** acquires immutable world/render projections and GRDG resource views. Render-frame completion is independent of simulation publication; the renderer acknowledges presentation separately.
- **Audio** receives short, bounded event/parameter messages through the existing audio command ingress. The real-time mixer never performs graph scheduling, application callbacks, blocking waits, or memory reclamation.
- **Networking** uses bounded decoded-message ingress and published replication projections; transport threads neither mutate simulation memory nor block a full-engine barrier.
- **Input** keeps the existing begin/sample/publish tick snapshot contract, with platform event delivery owned by the native window/input system.

A `SIRTaskGraph` pass specifies exact canonical resource-range read/write hazards. The graph compiles RAW, WAR and WAW dependencies and submits **one complete atomic batch** to the existing `SIRTaskScheduler`. Tasks not ordered by a hazard or explicit edge can execute concurrently. `MAIN` tasks are pumped by the application owner, `CPU` tasks by the scheduler workers, and `RENDER` / `PHYSICS` provider tasks by their registered provider owners. The application does not call `pumpProvider()` from the main thread. Arbitrary `SIREngineSystem::update()`/`fixedUpdate()` callbacks are not dispatched to the real-time `AUDIO` executor.

### Implemented components

| Component | Source | Contract |
|---|---|---|
| `SIRTimelineCoordinator` | `Source/Core/Scheduling/SIRTimelineCoordinator.{h,cpp}` | One-owner fixed-step accumulator, bounded catch-up, frame revision, optional presentation backpressure, independent presenter acknowledgment |
| `SIRPublishedState<T, Capacity>` | `Source/Core/Scheduling/SIRPublishedState.h` | Bounded 2–15-slot immutable projection with sequence-tagged atomic publication; no allocation after construction |
| `SIRStateReadLease<T, Capacity>` | Same | Reader CAS-pins a published slot and verifies the exact publication token before dereferencing; owner defers reuse until release |
| `SIRSubsystemCommandQueue<T, Capacity>` | `Source/Core/Scheduling/SIRSubsystemCommandQueue.h` | Bounded MPSC ingress via `SIRMutex`, monotonic admission sequence, timestamp/target tick, stable eligible drain; no callbacks while locked |
| `SIRSimulationSchedule` | `Source/Core/Scheduling/SIRSimulationSchedule.{h,cpp}` | Wraps existing `SIRTaskGraph`/`SIRTaskScheduler` without a second scheduler; all-or-nothing submission, nonblocking observation, cancellation and terminal cleanup |
| `SIRApplication` | `Source/Core/SIRApplication.{h,cpp}` | Optional timeline/task-schedule attachment, access declarations, executor/priority/dependency configuration, asynchronous scheduled tick begin/poll/cancel while preserving synchronous lifecycle APIs |

### Application execution

1. Initialize `SIRMemoryManager`, `SIRTaskScheduler`, and `SIRSimulationSchedule` (graph capacity must cover registered systems, their accesses and edges). Initialize `SIRTimelineCoordinator` with an explicit fixed-step nanosecond duration, maximum catch-up steps, and maximum frames ahead; `nMaxFramesAhead=0` disables presentation throttling for headless usage.
2. Before `SIRApplication::initialize()`, register systems, attach the timeline and schedule, and configure system executor, access declarations, priority and/or explicit dependency edges. **Unconfigured systems stay serialized** relative to all other registered systems, preserving their historical ordering. Configured systems are permitted to overlap only under the application's declared hazard contract.
3. Call `beginScheduledTick(delta, false)` to sample/publish input, open a timeline frame and submit the variable-update pass graph. It returns when the batch has been admitted, not when completed. Continue pumping relevant owner executors and call `pollScheduledTick()` until `SUCCEEDED` or `FAILED`. `PENDING` includes deferred GPU/physics provider work. No new scheduled tick can start while the old one is active.
4. On `SUCCEEDED` the application publishes a new frame revision and updates its frame count and elapsed time. The physics owner may independently run `beginScheduledTick(fixedDelta, true)` when `timeline.canAdvanceFixed()` permits it; after success, the timeline records one completed simulation tick. Use `SIRTimelineCoordinator::getSnapshot()` for the resulting frame/tick/revision/alpha.
5. A renderer acknowledges completed presentation with `acknowledgePresentedFrame(frame)`. Backpressure applies only when configured. The outstanding state-read leases and GRDG read scopes protect their own lifetimes; presentation acknowledgment does **not** free state that readers still hold.
6. Shutdown requires all task callbacks and deferred-provider completions to reach terminal state and all read leases to be released. `SIRApplication::shutdown()` returns `BUSY` while a scheduled tick is outstanding. Cancel first, allow providers to acknowledge completion, then poll to terminal and shut down.

The old `tick()` and `fixedTick()` entry points remain synchronous for existing consumers. New code explicitly chooses `beginScheduledTick()` / `pollScheduledTick()` to obtain job-graph execution; old systems are never silently run on worker threads.

### State-publication lifetime proof

The writer alone calls `beginWrite()` / `commitWrite()`. It may only write into a **closed slot with zero active pins**, never into the currently published slot. A publication opens its fully written slot and atomically stores a new 64-bit `(revision, slot)` token; the previously published slot is then closed for new readers. Readers load a token, increment its slot pin only if it is open, and re-read the token. They can dereference the state **only if both tokens match**. On mismatch, the pin is released and acquisition retries. A slot is reusable after it has been closed and every pin has drained; the revision prevents slot-index ABA. The protocol currently uses sequentially consistent `std::atomic` operations to keep the lifetime proof portable across Linux/Windows x64 and ARM64.

A full projection uses `Capacity * sizeof(T)` storage; this is intended for *compact, trivially copyable public projections*, not copying a physics broadphase or the complete mutable world. The writer may be unable to publish when every non-current slot remains pinned; `beginWrite()` reports failure without overwriting readers. It never waits for a slow renderer or network consumer. The owning subsystem decides whether to defer, coalesce or drop its **new** revision.

### Commands and real-time paths

Commands must be trivially copyable value descriptors (handles, fixed-width numeric data and generation/revision fields). Producers call `tryEnqueue(command, targetTick, timestampNs)`; when capacity is exhausted it returns `false` and does not allocate or drop a prior command. The consumer drains eligible commands into **caller-owned output storage**. Later eligible commands can bypass earlier *future-tick* commands while preserving the global ingestion sequence among drained commands. Producers do not invoke destination subsystem callbacks. The mutex is confined to metadata/copying, not executing commands or waiting for dependent work.

This particular MPSC queue intentionally uses a short native `SIRMutex`: it is suitable for main/network/physics command ingress, **not** for a hard real-time audio callback. Retain the audio system's already established real-time command ingress and FMOD/provider thread contracts rather than replacing them with this queue.

### Dependency and failure semantics

- `SIRSimulationSchedule::begin()` creates, compiles and submits one complete graph. Failed admission resets the builder without executing a partial batch; caller-owned contexts remain live through terminal `finish()`.
- If a task reports failure or cancellation, its dependents fail/cancel through `SIRTaskScheduler`; the application does not commit an unsuccessful frame revision.
- Once input has been committed, its tick identity is not reused even if task admission fails. An input pump rejection preserves the legacy application tick-count contract.
- A registered provider owner must call `completeProvider()` for deferred provider work, including after cancellation. The application must keep task contexts alive until `finish()` succeeds.
- The timeline clamps fixed-step catch-up; excess accumulated time is counted in `nDroppedTimeNs`. A canceled frame restores its newly accumulated delta when no fixed step was committed.
- Backpressure is per-frame and optional, not a global thread rendezvous. Frame count and simulation tick are distinct.

### Integration boundaries

At this baseline, `Source/Physics/` and `Source/Networking/` have not yet been implemented. Their command types, provider ownership and snapshot projections must be defined by the eventual subsystem owners, using these foundation contracts. Renderers retain `SIRFrameGraph` and GRDG resource lifetime semantics; audio retains its existing command/mixer threading. The foundation adds no speculative GPU fence callbacks or socket transport API.

### Validation required

- Linux GCC and Clang C++17 builds; Windows x64/ARM64 Clang cross-builds; Windows x64 execution under Wine; native Windows testing remains a separate gate.
- Repeated concurrent snapshot publication/read-lease retention, stale slot reuse and bounded exhaustion; `std::atomic` ordering under ThreadSanitizer.
- MPSC producer concurrency, target-tick filtering, full queue and output-boundary behavior.
- Task graph write/read hazards, explicit edges, provider safe-point completion, failures/cancel and terminal release; legacy application lifecycle and input regression tests.
- Optional frame-ahead throttling, catch-up limits, fixed-step progression and canceled-frame behavior.

### Patch delivery

Changed source/headers, CMake registration, this architecture document, and the new integration test are delivered as a **changed-files-only archive**, retaining their repository-relative paths. Build outputs, compiler toolchains, test logs, and duplicated unchanged source are excluded.

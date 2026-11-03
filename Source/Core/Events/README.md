# SIREngine Core Events: migration and bounded high-throughput variants

This directory contains the native C++17 event migration from NomadFramework plus throughput-oriented event transport and phase-delivery specializations. SIREngine-owned interfaces live in the global namespace. All fast-path buffers are embedded in their owning objects; the owning subsystem is responsible for constructing those objects from the appropriate SIR memory domain. No queue or dispatcher allocates memory while delivering events.

## Delivery choices and actual ownership

| Component | Producer/consumer contract | Purpose |
| --- | --- | --- |
| `SIREvent<T>` | One owner thread; subscriptions may change recursively | Dynamic synchronous typed events |
| `SIRStaticEvent<T>` | Subscribe before `seal()`, then publish against immutable listeners | Low-footprint, read-only signals; no unsubscribe |
| `SIREventStream<T>` | Owner-thread double buffer | Same-thread deferred events, batched append |
| `SIRDeferredEventChannel<T>` | Owner-thread signal + stream | Deferred typed fanout |
| `SIRSPSCEventRing<T>` | One producer, one consumer | Fixed-capacity ring with bulk cursor publication |
| `SIRAsyncEventChannel<T>` | One producer and one subscriber owner | Cross-thread buffered signal with batched publish/pump |
| `SIREventMailbox<T>` | Each registered token has exactly one producer thread, one owning consumer | Per-producer SPSC rings and fair batch draining |
| `SIRMPSCEventQueue<T>` | Concurrent producers, one consumer | Optional shared ingress; bounded sequence-tagged slots |
| `SIREventPhaseQueue<T>` | Owner thread, externally assigned phase IDs | Phase-specific buffering; validate generations on delivery |
| `SIREventQueue<T>` | Owner thread | Six-priority FIFO (original indexed priority scan) |
| `SIREventScheduler<T>` | Owner thread | General-range indexed min-heap timers |
| `SIREventTimingWheel<T>` | Owner thread | Dense, short-horizon timers in indexed fixed buckets |
| `SIREventRegistry` | Mutex-protected cold metadata | Generational event type handles and descriptor copies |

`SIRSubscriptionGroup` and `SIREventFilter` remain optional owner-thread helpers. The API does not use runtime namespaces, `Variant`, RTTI-based event dispatch, or engine-unowned queue nodes.

## Producer-local routing and bulk publication

Prefer `SIREventMailbox` when producer identities are known before workers start. Call `registerProducer()` on the owner thread for each worker, then `seal()` before launching worker threads. Every token must be used by exactly one producer thread. A producer calls `publishBatch()` to transfer a prefix of an array; the owner calls `pump()` with a global budget and per-producer quantum. The round-robin cursor is owner-only; it is not a globally ordered queue. `publishBatch()` returns the accepted prefix, so callers must retry or handle the remaining elements. For the shared-MPSC fallback, retry `SIR_EVENT_ENQUEUE_FULL` according to subsystem policy.

The SPSC ring now copies up to two contiguous segments per batch across wraparound and commits one release-store cursor update after the batch. The matching consumer uses one acquire load when it needs more data, then one release-store cursor update per batch. `drainBatch()` reserves ring storage until callbacks complete, rejecting recursive drain and dequeue from inside its callbacks; the producer cannot overwrite the uncommitted buffer. `stop()` prevents new operations that begin after stop is observed. Join all producers before destruction; operations that started before stop may finish.

`SIRMPSCEventQueue` uses one sequence number per slot and compare-exchange slot reservation, allowing concurrent producers without per-thread registration. It has higher contention and per-event synchronization cost; prefer it only when the source topology requires shared ingress. An in-flight producer reservation can delay the consumer from observing later committed slots. No unbounded queue growth or hidden heap allocator is used.

## Immutable signals and generational targets

`SIRStaticEvent` owns only contiguous callback records, a count, and sealed state. After sealing, its table never changes. Readers may publish from different threads only after initialization has been synchronized before those readers start; callback targets must themselves be safe for the chosen publishing concurrency. `SIREvent` remains the mutable, owner-only alternative with immediate unsubscribe suppression and deferred subscriber compaction.

`SIREventPhaseQueue<T, PhaseCount, Capacity>` uses one shared, compile-time bounded slab across all phases. A `sirEventTargetRef_t` contains a 64-bit logical ID plus generation and domain. The owning subsystem supplies a validator callback during phase delivery. A non-empty target is dropped if no validator is supplied or the owning subsystem says its generation is stale. The queue never owns referenced resources. The GRDG is not implemented in the `develop` baseline used for this migration, so the validator is a deliberately unbound integration seam: no claim of real GRDG resource publication is made.

Phase callbacks can enqueue into the same phase; accepted entries are processed on the next phase drain. An in-flight event keeps its slab slot until its callback returns, so when the shared slab is completely occupied, a reentrant enqueue returns false until a slot is reclaimed. Phase IDs are assigned by the owning subsystem, not hardcoded to a game-specific frame schedule. The registry continues to provide generation-checked *event metadata*; resource identity is validated by the resource authority, not the registry.

In the local x86-64 build, a four-phase queue with 256 TOTAL slots occupies 6,232 bytes. The earlier double-buffered prototype occupied 49,224 bytes with 256 slots PER phase/per buffer. These figures use different capacity contracts: compare committed bytes again under the actual subsystem peak-occupancy requirements. The target-first record layout occupies 24 bytes for a 4-byte payload.

## Timers and priorities

`SIREventTimingWheel` is a specialized alternative to `SIREventScheduler`. Its maximum delay and periodic interval are strictly below the configured bucket count. It supports cancellation with generation-safe handles, bounded pump budgets, callbacks that cancel other callbacks, and safe handling of long elapsed-time jumps. It skips missed recurring beats. Keep the min-heap for long-horizon and sparse timers. The six-priority queue retains its original indexed scan. A candidate occupied-priority bitmask regressed common single-priority operations in local same-workload benchmarks and was rejected. Ordering within each priority band remains FIFO. High-priority work can starve low-priority work if the consumer budget is insufficient; subsystems choose their own fairness policy.

## Verification scope

`Tests/Unit/SIREventGoogleTests.cpp` exercises baseline migration behavior. `Tests/Unit/SIREventOptimizationGoogleTests.cpp` adds batch wraparound, high-contention four-producer stress, static seal, stale-target rejection, nested drain protection, timing-wheel cancellation/reentrancy, timestamp extremes and a randomized heap-versus-wheel one-shot differential check. `Tests/Benchmarks/EventBenchmarks.cpp` registers dynamic/static dispatch, SPSC batch, shared MPSC, per-producer mailbox, heap and wheel benchmark cases. The local handoff contains separate Clang/ASan/UBSan, GCC/TSan and CMake smoke evidence; native repository/Windows CI are separate acceptance gates.

## Additional CPU and memory variants

The following specializations address different workloads without introducing a
single large polymorphic event dispatcher:

- `SIRDirtyEventSet<Capacity, TrackGeneration>` coalesces repeated dirty IDs in
  a compact bitset. `TrackGeneration=false` avoids the generation table when
  the owner already validates object identity; `true` retains the most recent
  generation per slot and accepts an owner-provided generation validator during
  drain. ID slots are 1-based. This set is **owner-thread only** and is not a
  cross-thread notification channel. A drain snapshots the bits; new marks
  issued by callbacks wait until a later drain. A callback's `clear()` aborts
  the remaining snapshot while retaining marks submitted afterward.
- `SIRChannelEventQueue<T, Capacity>` stores one callback/context pair for the
  entire channel instead of one pair per event. It is an owner-thread FIFO,
  supports bounded batches and prevents reentrant `pump()`; a callback's
  `clear()` cancels outstanding work while keeping newly enqueued work.
- `SIRExternalEventQueue<T>` stores only cold metadata (48 bytes on the tested
  x86-64 ABI). The owning subsystem supplies a live typed array and capacity,
  ideally allocated by `SIRMemoryManager` in the appropriate domain before
  worker startup. The queue never obtains or frees memory; the storage owner
  must retain the array until `shutdown()` and until no callbacks execute.
- `SIREventPhaseQueue<T, PhaseCount, Capacity, TTarget>` permits either the
  default 16-byte `sirEventTargetRef_t` or the 8-byte `sirCompactEventRef_t`.
  Use the compact form where phase/domain ownership already resolves the
  resource domain. Target generations are checked by the external owner before
  each callback. A phase pump can now take a maximum record budget; remaining
  detached work stays ahead of records queued during callbacks.
- `SIRSPSCEventRing<T, Capacity>` places each side's owned index and cached
  remote index together on its own cache line. The consumer may acquire two
  read-only contiguous spans and release an arbitrary prefix with `commitRead`,
  or use `drainSpans` to process them automatically without an output copy.
  Uncommitted spans continue to occupy ring capacity and must not outlive the
  ring or be passed to an unrelated consumer thread.
- `SIREventTimingWheel` now tracks occupied buckets in a bitmap and skips
  unoccupied ranges using word operations. This addresses the sparse-timer
  regression observed when advancing dozens or hundreds of empty ticks.
- `SIRAsyncEventChannel<T, Q, L, true>` and
  `SIRDeferredEventChannel<T, Q, L, true>` select **opt-in** subscriber-interest
  filtering. Unobserved event publication is discarded; late listeners do not
  receive earlier notifications. Default `false` retains buffered semantics.
- `SIREvent`, `SIRStaticEvent`, the SPSC ring, stream, priority queue, phase
  queue, scheduler and timing wheel avoid eager initialization of unoccupied
  payload slots. They still initialize all ownership, generation, and free-list
  metadata that controls lifecycle correctness.

The memory-owner rule is unchanged: queue storage and event objects are
constructed in SIR-controlled domains, and those domains remain responsible for
reclamation. No event system stores a raw, owning pointer to a GRDG resource.

## Evidence and dependencies

Local reference measurements and standalone repeatable benchmark sources are
included in the optimization handoff. SPSC cache-line grouping and remote-index
caching follow the designs described in Meta/Folly `ProducerConsumerQueue` and
Erik Rigtorp's `SPSCQueue`; the timer bucket occupancy map follows the Linux
kernel timer-wheel approach. These are algorithmic inspirations, not vendor
code copies. Tests include randomized differential coalescing/FIFO validation,
SPSC cross-thread sequence stress, cancellation and timestamp extremes,
reentrant callbacks, queue saturation, cold storage sizing and allocation
plateau checks.

Engine-wide job wakeups remain owned by the actual scheduler's wait protocol.
Implementing a condition-variable wake from event publication without the
scheduler's sleeping/armed handshake would permit lost wakeups; no such
unverified coupling is introduced in the event layer.

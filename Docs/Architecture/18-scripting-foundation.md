# 18 — Shared scripting foundation (initial native implementation)

**Implementation stage:** language-neutral scripting control plane with a bounded owner-lane scheduler. No language VM, GDScript lowering, Mono host, AngelScript/Lua bridge, GraphVM, native C++ module loader, LLVM worker or GRDG program loader is claimed by this milestone.

## Native targets

`SIRScripting` is a separate C++17 static library linked against `SIR::Core`. The `SIR::Scripting` CMake alias is consumed by `SIRScriptFoundationTests`. No compiler/VM vendor dependency is linked into the common API.

The public owner is `SIRScriptServer`, which constructs `SIRScriptBindingRegistry` before `SIRScriptRuntime` and holds the registry for the lifetime of the runtime. Language backends register one `sirScriptBackendAPI_t` function table (current ABI revision `SIR_SCRIPT_BACKEND_ABI_REVISION`). The backend owns its opaque program/instance tokens, VM heaps, stacks and garbage-collector roots. The common runtime only holds generation-checked slot handles, program reference counts, registrations and event subscriptions.

`SIRScriptBindingRegistry` validates typed native descriptors during registration, refuses duplicate IDs and invalid data layouts, and publishes one immutable schema. Native calls validate the ID, signature revision, frame sizes and pointer alignment before invoking their registered thunk. Events are separately declared with fixed payload type IDs and validated before dispatch. All language adapters must marshal their private values into these shared typed frames.

`SIRScriptRuntime` registers, starts and stops language backends, loads/unloads immutable program revisions, creates/initializes/destroys instances and invokes prebound method IDs. Successful initialization has exactly one shutdown callback on destruction. A failed initialization destroys backend-private partial state without invoking script shutdown. Program unloading/VM shutdown are rejected while instances/programs remain. Reused host slots have incremented generations; stale references are rejected. Destroying an instance revokes its event subscriptions.

## Ownership and execution restrictions

The control plane captures its owning `std::thread::id` during construction and rejects calls from other threads with `SIR_SCRIPT_BUSY` before touching mutable state. All scripting control-plane calls are single-owner operations. The immutable published binding registry is expected to be constructed and published on that same initialization lane. The fixed-capacity arrays are allocation-free bootstrap storage and are *not* the final SIRMemory budgeting scheme. There are no concurrent publishers, coroutine scheduler or background script calls in this slice. Backend calls are synchronous and marked `noexcept`. Backend callbacks cannot recursively mutate the host control plane; reentrant entry points return `SIR_SCRIPT_BUSY`. During event dispatch, a subscription may be removed, but no new subscription or destruction is permitted. Exceptions from a backend are forbidden across the ABI.

This bootstrap can accommodate GDScript, C#, Lua, AngelScript, C++17 and GraphVM as distinct backend IDs. Neither the VM implementations nor backend-specific binding adapters are provided here. Future scheduler/GRDG integration must retain the existing host handle and immutable-schema semantics and replace fixed-capacity backing with SIRMemory-accounted owner storage, without adding duplicate binding registries.

## Deferred script scheduler (implementation slice 2)

`SIRScriptScheduler` now provides a per-host, owner-thread queue with two explicit lifecycle phases, `EARLY` and `LATE`. It queues either typed events addressed to all subscribers or fire-and-forget method invocations addressed to generation-checked script instance handles. Both operations copy the argument bytes before returning, eliminating borrowed-frame lifetime hazards. `SIRScriptRuntime::enqueueEvent`, `enqueueInvoke`, `cancelScheduled`, `pumpPhase` and `pendingCount` form the public deferred-dispatch API. `enqueueInvoke` requires a registered argument TypeID. Type descriptors have explicit `SIR_SCRIPT_TYPE_PLAIN_BYTES` and `SIR_SCRIPT_TYPE_NO_POINTERS` flags. Deferred payloads must have both; pointer-bearing or unclassified frame layouts are rejected, while immediate borrowed-frame calls remain available. The type registrar is responsible for truthful layout flags; complete field-level type schema verification is a separate contract. The backend ABI revision is 3 for this unpublished foundation (revised in slice 3 to consume immutable cooked program views). All language backends receive the same canonical `sirScriptReadFrame_t` ABI at delivery. The scheduler never executes a language-specific interpreter directly.

The initial queue stores at most **64 total pending tasks**, with **256 bytes** of inline payload and a **64-byte maximum alignment**. Oversize or over-aligned event/argument frames fail with `SIR_SCRIPT_CAPACITY_EXCEEDED` instead of allocating outside the memory domain. The queue is not a replacement for future SIRMemory-backed capacity planning. Each queue slot has a generation-checked cancellation handle, immediately recyclable on cancellation. FIFO is preserved within each phase, and caller-provided per-pump budgets bound execution. A monotonic enqueue sequence bounds a pump to tasks present at its start; callbacks that cancel older work and enqueue new work cannot accidentally run their own additions in the same pass.

Instance handles are checked again at deferred delivery. A destroyed instance's method call is counted as stale and skipped; a queued broadcast only targets subscriptions still active when delivered. Subscriptions may be removed synchronously from their callbacks; callbacks may enqueue new tasks for a later pump. Full runtime reentrancy and recursive pumping remain blocked. `pumpPhase` reports processed tasks and skipped stale invocations, and returns the first backend failure while continuing to process its budget. Deferred method invocations use an empty return frame and therefore require backend methods with fire-and-forget semantics. The scheduler does not yet serialize arbitrary object graphs, managed references, dynamic-sized values, or coroutine continuations.

`SIRScriptRuntime::shutdownAll()` now clears queued work before destroying script instances, unloading programs and stopping active backends; subsequent calls are idempotent. `unregisterBackend()` permits stopped/unloaded backends to be removed so their external module can subsequently be unloaded. During shutdown, new deferred work is rejected even if a backend shutdown callback attempts to enqueue it. The caller is still responsible for explicitly invoking shutdown before destroying the server; no destructor is claimed to drain external VM state.

The tests exercise: event payload copy isolation, early/late phase ordering, cancellation and immediate slot reuse, stale generations, queue saturation, oversize/over-aligned and pointer-unsafe frame rejection, cross-thread owner checking, cancellation/enqueue during pump, first-error propagation, cross-backend broadcasts, idempotent complete shutdown and independence of per-backend lifetimes.

## Follow-on integration contracts

1. Freeze versioned byte-level type/argument metadata and stable MethodID/FunctionID generation across compilers.
2. Extend the implemented single-owner bounded scheduler with SIRMemory-accounted variable-size payloads, explicit producer-to-owner ingress, engine phase adapters and coroutine continuations.
3. Implement the concrete GRDG-backed provider for the new program-view/lease API and resource cancellation.
4. Implement C++17 native module registration, baseline AOT and LLVM bitcode cooking as one scripting backend.
5. Implement `SIRScriptCodeManager` for native-code generation publication and retirement. The separate `SIRNativeOptimizer` process must only deliver validated artifacts, not own live instances or engine state.
6. Implement GDScript, AngelScript, Lua, Mono and VisualGraph adapters against the same backend and binding contracts.

## Tests and required gates

`SIRScriptFoundationTests` covers schema publication, duplicate IDs, invalid alignment, mismatched signature/revision/frame, event schemas, startup, duplicate programs, Init rollback, exactly-once Shutdown, unload blocking while referenced, subscription delivery/unsubscribe, stale generations and stopped backends.

Run the repository CMake/FASTBuild preset and CTest on Windows and Linux. The standalone compiler smoke test may validate the new translation units but does not certify the integrated engine build, backend hosting, concurrency, or production memory budgets.

## Immutable cooked-program leases and schema identity (implementation slice 3)

The shared runtime now requires a `sirScriptProgramSourceAPI_t` for loading any backend program. Its acquire callback returns a versioned `sirScriptProgramView_t` with program ID, exact revision, cooked format ID, immutable byte pointer, byte length and alignment, plus a nonzero opaque lease token. The provider controls the resource storage and lifetime; this API is the native integration seam for a future GRDG resource reader. It is not a GRDG implementation or a mock resource registry shipped with the engine. The runtime validates the returned revision, identity and frame metadata before invoking the selected backend's `pLoadProgram` with the protected view. Different language backends can accept different cooked `nFormatId` values without changing the shared resource contract.

After `pLoadProgram` succeeds, the runtime stores the lease token alongside the generation-checked program handle and keeps it pinned throughout the backend program lifetime, including all live instances. `unloadProgram` first invokes the backend unload callback, then releases the resource lease. Failed backend loads and invalid source views immediately release successful acquisitions; a failed source acquisition must leave no outstanding lease. Source swapping/removal while any program remains loaded is rejected. The program source must outlive all programs, and callers must invoke `shutdownAll` before destroying external source/backend contexts. The provider must treat successful acquisition tokens as unique until release, and must not invalidate returned views before release.

The registry's `schemaFingerprint()` reports a deterministic 64-bit ABI identity after `publish()` and zero before publication. Publication canonically sorts types by TypeID, functions by FunctionID and events by EventTypeID, then hashes their named public schema fields (including binding revisions and owner lanes) in a fixed byte order. The digest excludes callback addresses, allocator addresses, implementation pointer values, C++ struct padding and registration order. It is a noncryptographic schema fingerprint, **not** a replacement for an artifact cryptographic digest or the full LLVM native-cache identity (compiler/toolchain versions, program source digest, target ABI/CPU, optimization settings and dependency closure still belong in that key).

New tests exercise: mismatched source ABI, missing source, source acquisition failures, bad program revisions/alignment, invalid lease tokens, backend-load rollback, mutation restrictions while pinned, exactly-once release after unload, retained resource leases across two registered backend IDs, and registration-order-independent schema fingerprints with invalidation when a type declaration changes. No VM frontend/compiler or platform-specific executable-code loader is added in this slice.


## Thread-safe event ingress and memory telemetry (implementation slice 4)

`SIRScriptIngress` owns one 64-slot, bounded MPSC ring per phase, each with an inline 256-byte, maximum-64-byte-aligned payload. Producers may call `SIRScriptRuntime::postEvent` concurrently **after** schema publication and before shutdown. The immutable binding registry validates event type, fixed size, alignment and the `PLAIN_BYTES | NO_POINTERS` classification before copying the payload; pointers and VM state never cross the worker boundary. The shared runtime does not execute script methods on worker threads. Its owner thread calls `pumpIngress(phase, budget, processed)` to publish events through the existing subscription dispatch. Each phase captures its insertion-ticket boundary at pump entry, preventing callback-created events from running in the same pump. Full queues return `SIR_SCRIPT_CAPACITY_EXCEEDED`; no hidden heap fallback or busy-loop is imposed on callers.

The ingress closes and drains at the beginning of `shutdownAll`, before backend destruction. Close waits for already admitted producers to finish their bounded publication; the runtime and published registry must outlive all producer calls, and callers must join producer threads before destroying the scripting host. Shutdown permanently closes that runtime's ingress. Per-phase ingress delivery and owner-lane scheduler delivery are independently budgeted; neither guarantees global FIFO ordering relative to the other.

`queryMemoryFootprint()` reports the fixed resident object bytes and the scheduler/ingress subobject sizes, plus pending owner and producer tasks. Calls from non-owner threads return an empty report. This is measurable bootstrap storage and provides a domain-accounting input; it **does not yet** allocate from SIRMemory, apply a configurable runtime memory budget, implement arbitrary-size payloads, or claim GRDG integration. Future SIRMemory-backed queues should retain the same event and phase interfaces while replacing the fixed backing.

New tests cover four simultaneous worker producers, overlapping producer/consumer dispatch under saturation, exact payload integrity, phase budgets and callback insertion cutoffs, full-queue rejection, capacity reuse, wrong-thread invocation prevention, pointer-bearing payload rejection and post-shutdown rejection.


## SIRMemory-routed deferred payloads (implementation slice 5)

The bounded owner-thread scheduler retains 64 task slots and its 256-byte inline envelope. `setPayloadAllocator` installs a versioned callback table for individually reclaimable overflow buffers, with explicit per-game maximum bytes (up to 1 MiB per task) and maximum alignment (up to 4096). Small payloads remain inline and do not call the provider. Large or over-aligned payloads are validated by the published type schema, allocated and copied before enqueue returns. Without an installed allocator they still fail with `SIR_SCRIPT_CAPACITY_EXCEEDED`; there is no global `new`, `malloc` or hidden heap fallback. Worker-thread ingress remains a separate fixed-size MPSC queue and never uses this owner-thread allocation API.

`SIRScriptMemoryRouter::makeAPI()` adapts an engine-owned `SIRAllocationRouter` to the callback ABI, requesting `SIR_LIFETIME_ARBITRARY` and preserving the route's backing tag and size in the opaque lease token. Its pool/buddy/TLSF configuration, domain assignment and accounting remain owned by the engine's SIRMemory setup. Both allocation and release occur on the runtime owner thread. The adapter and router must outlive the scheduler and all callback activity. The engine is responsible for configuring the router before installing the adapter and draining scripts before shutting memory down.

The scheduler retains ownership until cancellation or shutdown, transfers an overflow lease to the pump when dequeued, and frees that lease after backend dispatch. A live dequeued task blocks allocator replacement until its callback finishes. Exact allocated bytes, active overflow allocation count, and peak bytes appear in `queryMemoryFootprint` without changing the backend ABI revision. Each overflow buffer has an explicit reclamation path; failure to allocate leaves queue occupancy unchanged. The scheduler destructor drains pending tasks, but the containing runtime's external program/backend shutdown order remains explicit.

Tests verify deep-copy isolation, alignment, allocation failure, a rejected provider ABI, cancellation, slot reuse, queue draining, in-flight leases, shutdown reclamation, oversize rejection and memory telemetry. The actual engine SIRAllocationRouter adapter requires the native SIRCore build to verify its backing-pool and thread-owner behavior; standalone smoke uses a tagged aligned-heap provider to exercise the scheduler independently.

## Language-neutral executable artifact activation (implementation slice 6)

`SIRScriptCodeManager` stages immutable, externally produced code artifacts through
`SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION`. Each artifact carries the producing
backend ID, program ID/revision, format ID, published binding-schema fingerprint,
CPU/target identity and opaque build fingerprint. Staging validates these
identities against the active published schema and configured target, acquires
one provider lease, and checks the returned immutable view's identity and
alignment. The build fingerprint is a cache key, not cryptographic content
verification; the external artifact provider owns byte-integrity verification.
No LLVM, JIT, language frontend, executable page mapping or process launch is
introduced in the shared manager.

Activation changes the revision available to **new** runtime program loads;
previously loaded programs and instances continue under their existing
runtime handles. The superseded artifact is retired and remains pinned until
all host leases finish, then releases its provider lease. Host lease tokens
are separately generated for each load and generation-checked against duplicate
or stale releases. Staged artifacts can be discarded, active revisions can be
retired, and shutdown rejects live runtime leases. Fixed capacity is 32
artifact records and 128 independent host leases; capacity exhaustion returns
an explicit status with no heap fallback. Staging, activation, retirement,
acquire and release share the script owner lane. Provider callbacks cannot
reenter the manager's mutable control plane.

`SIRScriptServer` now constructs the code manager between the binding registry
and runtime, exposes `codeManager()` and offers `useCodeManagerSource()` to
connect its source adapter to `SIRScriptRuntime`. Configure the code manager's
artifact provider, publish the binding registry, stage an artifact, activate it,
then load the chosen active revision through the runtime. The server destructor shuts down the runtime before releasing the manager's
provider leases. Join all event producers before server destruction; the whole
shutdown sequence must execute on the scripting owner thread.
The code manager retains artifact bytes by lease rather than copying them;
it cannot verify or free memory from a provider that violates its contract.

Tests cover schema/target mismatch, invalid acquired views, acquisition rollback,
producer reentrancy, duplicate revisions, slot and lease saturation, wrong-thread
access, stale handles, duplicate lease release, activation/retirement ordering,
backend load-failure rollback, and coexistence of old loaded programs with
new active revisions. Native compiler/optimizer workers and persistent artifact
cache are separate integration stages.

## Persistent artifact cache and optimizer wire contract (implementation slice 7)

`SIRScriptArtifactCache` is a language-neutral, single-owner cache storing cooked bytes as
`<cache-root>/<64-hex-recipe-sha256>.sirbc`. The host provisions a private cache directory
and chooses a maximum artifact byte size at configuration time (at most `UINT32_MAX` per
entry). `makeKey()` hashes an **externally canonicalized, versioned build recipe**. That recipe
must contain frontend/compiler and optimizer versions, source and dependency digests, binding
schema fingerprint, target ABI/CPU capabilities, code format and optimization parameters;
this slice does not define or silently invent frontend-specific recipe serialization. A
cache entry records the existing `sirScriptCodeArtifact_t`, recipe SHA-256, separate cooked
payload SHA-256, payload byte length and alignment. Its binary header uses explicit
little-endian fields, a magic marker, and format revision 1; no C++ structure padding,
pointer addresses or process-local tokens are persisted.

The store writes into a per-process, monotonically numbered exclusive temporary file,
flushes and synchronizes it, then atomically renames the completed record into place.
On Linux it additionally synchronizes the parent directory; on Windows it uses
`MoveFileExA` with write-through and replace semantics. An existing recipe key with
different artifact metadata or bytes is rejected as a deterministic-build conflict.
Reopening a new cache object reads the previous file without an in-memory index. Reads
require a caller-owned aligned destination and check the complete descriptor, payload
length, exact EOF and SHA-256 before the caller may consume the returned bytes. The
caller can explicitly erase malformed entries and rebuild. No implicit allocator,
heap residency, archive scan, eviction or disk-quota manager is introduced. Different
processes **must coordinate cache writes for a given key**; the root must be trusted and
not writable by untrusted users. No executable page mapping or runtime code loading is
performed here.

`SIRScriptOptimizationProtocol` defines portable, fixed-size revision-1 envelopes for an
external native-code optimizer: **160-byte requests** and **200-byte completions**. Both
carry a wire header and SHA-256 checksum; requests carry job identity, backend/program
revision, code format, schema, target, build fingerprint, canonical recipe key, input byte
length, and optimization tier. A successful completion carries the cache artifact
descriptor (rather than arbitrary executable pointers). `validateCompletion()` checks
job identity, the complete expected code identity, and the recipe digest; the host must
then call `SIRScriptArtifactCache::fetch()` to verify the cooked bytes before passing them
to an engine-owned immutable provider and `SIRScriptCodeManager::stage()`. Failure replies
carry a status and no artifact. Checksums detect accidental transport corruption;
**they do not authenticate an untrusted worker**. Launching workers, OS IPC transport,
source-file ownership, C++/LLVM compilation, cache eviction and GRDG-backed loading
remain separate implementations.

`SIRScriptArtifactCacheTests` exercises FIPS SHA-256 vectors, durable writes/reopen,
byte-for-byte read verification, recipe conflicts, corrupt payload refusal, explicit
recovery, wrong-thread calls, alignment and per-entry limits. The optimization protocol
test round-trips requests and replies, verifies cache-backed completion, rejects mismatched
job/target/recipe identities, invalid status values, unknown fields and corrupted wires.
Both tests are registered with native CMake/CTest alongside the existing foundation and
code-manager suites. The focused smoke builds do not prove a full engine link or a real
optimizer process.

## Verified cache-backed program source (implementation slice 8)

`SIRScriptCachedProgramSource` is the owner-thread bridge from the persistent
`SIRScriptArtifactCache` into the existing `sirScriptProgramSourceAPI_t` consumed
by `SIRScriptCodeManager`. Registration records a complete cooked-artifact
identity and SHA-256 descriptor. When the code manager stages a revision,
the bridge loads the artifact through the cache's **verified** `fetch()` method
into a caller-supplied, individually reclaimable engine allocation; it does
not create an executable mapping. Loading fails closed on missing files,
invalid headers, damaged bytes, alignment failures, allocator refusal and
resident-budget exhaustion. No fallback process heap is used.

The bridge accepts `sirScriptPayloadAllocatorAPI_t`, which can be provided by
`SIRScriptMemoryRouter::makeAPI()` with the existing `SIRAllocationRouter`.
The router and cache must outlive the provider. The provider must outlive
`SIRScriptCodeManager`, and the code manager must outlive loaded runtime
programs. A code-manager stage pins a provider lease; additional provider
acquisitions share the same immutable allocation until the final lease is
released. Retired program revisions remain readable while prior runtime loads
are live. Last-release reclamation is immediate and occurs on the script owner
thread. The bridge reports live and peak resident bytes, registered artifacts,
resident artifacts and active leases. It limits registration to 32 artifacts
and provider leases to 128. These limits are independent of the code manager.

A typical integration sequence is: configure the disk cache; configure the
provider using a routed allocator and resident byte budget; register the
verified optimizer completion descriptor; publish the binding registry;
configure the code manager with the provider's `makeProgramSourceAPI()`;
then stage/activate the artifact and let the runtime acquire its revision.
Shutdown must drain runtime instances and loaded programs first, shut down the
code manager next, and only then shut down the provider, allocator and cache.
The bridge does not own the cache, allocator or their memory domains. It
retains the existing script owner-thread contract; cross-thread producers
must use the event-ingress boundary instead.

`SIRScriptCachedProgramSourceTests` covers byte integrity, duplicate and
conflicting registration, pin counts and stale releases, runtime revision
swapping, missing-cache rollback, allocator failure, resident-budget pressure,
wrong-thread rejection, and resource cleanup. This is a verified cache
**provider**, not a native optimizer process, code executor or disk eviction
manager. Those remain separate implementation stages.


## Optimizer coordinator (implementation slice 9)

`SIRScriptOptimizationCoordinator` is the bounded (64-job) owner-thread coordinator
for the external compiler/optimizer. Its transport is a versioned, host-supplied
child-process/IPC callback table: start, stop, atomic send of one 160-byte request,
and nonblocking receive of one 200-byte reply. The transport owns process launch,
pipe framing and watchdog; this module does not claim to launch an OS process.
Requests receive monotonic process-lifetime job IDs; generation-checked host
handles and terminal records are separate identities. Cancellation before dispatch
skips the worker; cancellation after dispatch keeps a tombstone until the reply
is drained or the worker stops. A malformed, unexpected or truncated wire reply
invalidates the worker and fails outstanding in-flight jobs. Bounded pump budgets
keep optimizer traffic off the frame's critical path.

For successful completions, the coordinator checks the reply against the original
request, registers its cache descriptor with `SIRScriptCachedProgramSource`, and
stages through `SIRScriptCodeManager`. Staging acquires verified payload bytes
from the persistent cache. Only then does optional activation make the revision
visible to future runtime loads. Stage/activate failures unregister the source
entry and discard staged code to avoid leaked ownership. A successful job
transfers its source/code handles to the caller, which must retire/unregister
those resources independently of the job record.

No frontend, bytecode VM, LLVM optimizer, physical worker process, IPC pipe
implementation, timeout watchdog or cache eviction policy is included here.
A worker needs a separate canonical input staging contract because the existing
fixed request record holds an input size and cache recipe key, not source bytes.
The planned gameplay language and SUIL both use this pipeline after their own
frontends produce canonical, versioned build recipes and backend-specific
artifacts. Their language semantics remain outside the coordinator.

## Asynchronous external optimizer process transport (implementation slice 10)

`SIRScriptOptimizerProcessTransport` is the concrete Windows/Linux child-process
adapter for the Stage 9 transport ABI. It launches a configured executable
without a shell, connects a dedicated stdin/stdout pipe pair, and runs one
writer thread and one reader thread. The owner-thread `send()` callback copies
one complete 160-byte request into a bounded 16-frame queue, while `receive()`
returns only complete 200-byte replies from another bounded 16-frame queue.
Both callbacks return `SIR_SCRIPT_BUSY` rather than waiting for OS I/O.
The worker must reserve stdout exclusively for canonical binary protocol
records and use stderr for diagnostic text. Requests and replies are checked
by `SIRScriptOptimizationProtocol`, which remains backend-independent.

The process configuration owns the executable path, up to eight copied
arguments, and an optional reply-progress watchdog deadline in milliseconds.
The watchdog fires only while requests remain outstanding. EOF, partial frame,
pipe errors, worker exit, and an expired watchdog return a transport failure;
`SIRScriptOptimizationCoordinator` then stops the worker and fails its
in-flight jobs. `stop()` terminates the child, unblocks and joins both I/O
threads, reaps its process and closes pipe handles before clearing queues.
On Linux, the writer thread blocks SIGPIPE so an exited worker cannot kill
the host; child creation uses `posix_spawnp` with file actions. On Windows,
`CreateProcessA` is used with redirected inherited pipe handles and the same
thread-owned blocking-I/O model. This implementation does not set process
priority or create a sandboxed worker security context.

The CTest fixture launches an actual worker process and tests segmented pipe
frames, worker failure/EOF, truncated replies, watchdog termination,
transport restart, and coordinator job completion over IPC. These are
protocol and lifecycle tests, **not** compiler tests: the fixture emits a
valid failure reply without generating optimized code. A production worker
must implement canonical input staging, compilation/optimization and
SHA-256-verified artifact cache publishing before returning success.

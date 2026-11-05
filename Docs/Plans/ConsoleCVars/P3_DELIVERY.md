# P3 — Typed CVar banks and atomic transactions

**Branch:** `feature/console-cvars-commands` · [P3 issue #278](https://github.com/SIGAAMDAD/SIREngine/issues/278) · **Status:** implemented, native CI pending.

## Runtime implementation

`SIRCVarRegistry` binds only real P2 `sirRegistrationHandle_t` CVar descriptors. The helper `sirCVarDefinitionFromCooked` borrows checked SIRC v1 default bytes from the caller's live cooked catalog; dynamic CVars can bind with an explicit typed default. CVar handles are independent 8-byte `{slot,generation}` identities, and every get/write checks the owning P2 descriptor generation.

All memory is caller-supplied and size-checked before initialization. Separate preallocated 32-bit/64-bit value banks store scalar effective state. UTF-8 values, overrides, staged transactions, pending latched values and old/new event snapshots each have separately budgeted, reclaimable fixed-stride storage. No heap allocation occurs in cached scalar lookups, preparation or commit. The owner thread is enforced explicitly.

Source precedence is ENGINE_DEFAULT < DEVICE_PROFILE < PROJECT_PROFILE < USER_ARCHIVE < COMMAND_LINE < SESSION_CONSOLE < EDITOR_PREVIEW < SERVER_AUTHORITY; within equal source priority, higher stable issuer ID wins and the latest revision wins for one issuer. Removing an override falls back to the surviving winner. Lower-priority history changes do not publish an effective-value event. READ_ONLY/INIT_ONLY/DEVELOPMENT/CHEAT/REPLICATED/SENSITIVE policies require matching explicit capabilities and phase/build rights.

`prepareBatch` copies the entire override history to caller-owned shadow memory, snapshots caller string arguments, validates the complete write batch and reserves all required event-ring credits before returning a single-use owner-thread token. `commitBatch` copies old/new event snapshots **before** publishing changed active/latched history and effective banks, increments one coherent revision and consumes reserved event credits with no fallible append. `cancelBatch` publishes nothing. Queue-full, bad permission, bad type, exhausted override slots or stale handles leave visible state intact. Safe-point/restart latched writes stage pending values without mutating current effective state; `applyLatched` uses the same preflight/event-credit protocol.

`peekEvent` and `popEvent` expose stable per-event snapshots until acknowledged. `pumpGroups` invokes each sink group once per effective revision, in owner-thread delivery, and prevents recursive mutation during dispatch. Lifecycle requires the caller to quiesce mutators/sinks before shutdown; source P2 module unload invalidates the related handles, and `unbind` retires value history.

## Verification evidence

- **PASS locally:** isolated SIRCVarRegistry + real P0 flag validator compiled with GCC 14 `-std=c++17 -Wall -Wextra -Wpedantic -Werror`; focused mock-P2 transaction test executed successfully; ASan+UBSan test binary executed successfully; authored C++ formatted with pinned clang-format 23.1.5.
- **Added native CMake/CTest:** `core.cvars.registry` exercises real P2 registration cooker, SIRS string pool, SIRC catalog and typed CVar banks; preflight/cancel/commit, precedence fallback, latched string snapshots, permission failures, full event credits, grouped sinks, stale dynamic module handles, caller memory sizing and wrong-owner-thread checks.
- **Pending:** native Linux/Windows exact-SHA engine link/CTest, exact-source style CI and all-platform acceptance. Local mock integration is not a substitute for native end-to-end testing.
- **Remaining milestones:** P4 command execution and P5 typed archive/Config integration; P6 consumer/sink wiring and P7 exhaustive branch coverage/benchmarks.

## Lifetime and memory contract

The caller allocates and retains all arrays, value banks and text slabs through `shutdown`. `nStringStride` is the maximum UTF-8 value length plus one terminator; each slab advertises its byte length, which is checked before initialization. Cooked default text pointers borrow the P2 catalog blob and must remain immutable and live for the registry lifetime. Event text remains stable until `popEvent` or `pumpGroups`. Calls are owner-thread-only at P3; cross-thread producer ingress is the responsibility of P4.

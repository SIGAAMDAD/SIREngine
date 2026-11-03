# Wave A: portable audio contracts

The owned audio interface is separate from FMOD/Wwise/OpenAL provider details.
The backend ABI is version 2. Audio commands and completions are fixed POD
messages; no provider SDK headers are part of exported interfaces.

Playback identity retains 32-bit slot, 32-bit generation and 64-bit epoch.
SIRAudioSystem uses one logical owner thread for playback handle changes and
completion processing. Global/emitter parameter commands may have multiple
producers, with producers quiesced before shutdown.

A bounded MPMC queue is used for submission. A dedicated critical queue
reserves capacity for stop requests. Full queues explicitly return
SIR_AUDIO_CAPACITY; accepted commands are never silently overwritten.
The subsystem allocates its slot tables and queues through a required
caller-provided sirContainerAllocator_t, allowing SIR_MEMORY_DOMAIN_AUDIO
binding via SIRContainerHeapAdapter.

SIRAudioMockBackend supplies deterministic playback and failure completions.
The mock is a test fixture only, not an audio playback provider. FMOD context,
event instance ownership, bank residency and SDK/device tests are later waves.

All API calls that mutate logical playing handles are owner-thread-only.
Shutdown requires every producer and backend callback to be quiesced.

## Command and completion guarantees

- Each accepted play/stop command receives an engine sequence number.
  Only matching play-start, play-failure, stop, and natural-finish records
  may transition the corresponding logical playback state.
- Reused playback slots increment their generation; restarting the audio
  system changes its epoch. Old completions and commands cannot mutate
  the new logical handle.
- A failed native **play** submission marks the occurrence failed. A failed
  native **stop** submission restores the previous state so the caller can
  retry. Failures of parameter commands do not invalidate an active voice.
- The normal queue and reserved stop queue are each preallocated,
  capacity-bounded, and processed with finite per-update budgets. If
  a stop cannot be queued, the API returns SIR_AUDIO_CAPACITY and leaves
  its state unchanged; the caller owns the retry.
- A backend returning an error from submit must not retain the borrowed
  command pointer or create native ownership that the error leaves
  unaccounted. Successful asynchronous submissions must ultimately
  report their corresponding completion. Callbacks submit fixed-size
  completion records; only the logical owner thread updates handle slots.
- Native providers must drain completions without silent loss. Wave A
  drains after each dispatch to exercise small-capacity completion queues;
  later providers may batch only with equivalent capacity guarantees.
- The 64-byte command, 32-byte completion, and 16-byte handle layouts are
  locked to ABI version 2 by compile-time checks.

## Scope remaining after Wave A contract and mock tests

Provider-neutral cooked manifest validation and immutable logical resource
lookup are implemented in SIRAudioResourceStore. Production cooking, FMOD
native lifetime, bank residency, GRDG integration and device recovery
remain separate implementation waves. No
FMOD/Wwise/OpenAL device playback is represented by mock test success.

## Wave A spatial identity and attribute transport

- Emitters and listeners use fixed-capacity pools with slot, generation and
  epoch identities. Initialization allocates both tables through the caller's
  allocator; a zero configured capacity disables that handle family.
- Registration is an owner-thread operation. Release commands use the reserved
  critical queue; release of an emitter with live playback occurrences returns
  `SIR_AUDIO_BUSY`. A provider release failure restores the handle for retry.
- Spatial snapshots are copied into the preallocated normal command queue,
  never borrowed from a gameplay caller. The backend ABI v2 `submitSpatial`
  callback receives the command and its 48-byte attribute payload together.
  Transport order is preserved relative to play and parameter commands.
- Listener identity is encoded in the wire command's spatial handle fields
  for listener-specific command types; the command type disambiguates it.
  The backend must not retain caller-owned callback argument pointers.
- Spatial input rejects NaN and infinity; coordinates and basis vectors are
  passed through without middleware-specific transformation.
- Release is a synchronous provider operation returning `SIR_AUDIO_OK` or an
  error; the portable core tracks pending release until update dispatches it.
  A provider must not acknowledge release before retiring its own references.
- The in-process queue packet is 112 bytes; the public wire command remains
  64 bytes and the transported messages contain no retained external pointers.

## Wave A cooked resource contract

- `SIRAudioResourceStore` validates a versioned, little-endian, provider-specific
  **cooked mapping** of logical packages, events, controls and package
  dependencies. The header includes build identity, content hash, provider
  identifier and required capability mask.
- Validation rejects schema/version/provider/endianness mismatch, invalid
  offsets and record sizes, hash mismatch, missing capabilities, duplicate or
  unsorted IDs, missing/cyclic package dependencies, and malformed or
  unsupported controls. No FMOD/Wwise/OpenAL API is called.
- A valid manifest is copied into a single caller-allocated immutable backing
  store. Playback-time lookups use sorted numerical IDs and binary search,
  with no provider SDK discovery, strings or heap allocation in the hot path.
- Manifest reload is transactional: failed validation keeps the previous
  snapshot; accepted reloads get a new 64-bit epoch and invalidate earlier
  event/package handles. Callers receive record **copies**, not borrowed
  memory pointers.
- Optionally bind `SIRAudioSystem` to a loaded resource store at initialize.
  The system checks known events and validates control ID, scope, supported
  provider binding, type and numeric range before submitting hot commands.
  The store must outlive the bound audio system and remain on its published
  epoch until that system shuts down.
- Tests cover valid and corrupted images, hash/provider/schema errors,
  missing and cyclic dependencies, out-of-budget allocations, rollback,
  handle staleness, and gameplay command validation. The source manifest
  cooker, GRDG publication and provider-native bank lifecycle are later work.

## Wave B native context

`FMOD/SIRFMODContext` now implements one-owner Studio/Core lifecycle,
initialization rollback, SDK/runtime version validation, optional driver or
no-sound selection, update, release and restart. Its callback-injected
controller runs an SDK-free lifecycle test. The real `FMOD::Studio::System`
bridge is linked only when `SIR_WITH_FMOD=ON`, together with a no-sound
runtime smoke test. See `FMOD/README.md` for ownership and verification.


## Wave C — FMOD event playback implementation

`FMOD/SIRFMODVoiceBackend` implements the portable audio backend ABI for
pre-registered native FMOD event descriptions, fixed-capacity voice slots,
natural completion and stop/release handling. The native bridge is compiled
only with `SIR_WITH_FMOD=ON`; SDK-free tests cover its owned lifecycle.
Events must be registered once their FMOD banks are resident. Bank/GRDG
integration, actual SDK execution, spatial updates and parameter bindings
remain separate acceptance work.

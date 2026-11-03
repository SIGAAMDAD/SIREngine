# Wave B — native FMOD Studio context

`SIRFMODContext` owns exactly one `FMOD::Studio::System` and borrows its
`FMOD::System` (Core). It never releases Core independently. Its operations are
restricted to one control thread; no SDK objects escape into `Source/Audio/`.

The lifecycle controller is compiled for all configurations and uses the
private, versioned `sirFMODNativeOps_t` bridge. The bridge is implemented by
`SIRFMODNativeApi.cpp` **only** when `SIR_WITH_FMOD=ON`, in which case the
installed licensed SDK supplies `fmod.hpp`, `fmod_studio.hpp` and matching
Core/Studio libraries. A deterministic test fixture exercises all lifecycle
transitions when the SDK is not installed.

`initialize()` creates Studio, borrows Core, checks Core's runtime version
against `FMOD_VERSION`, optionally selects a validated driver or no-sound
output, then initializes Studio and its borrowed Core exactly once. It enables
deferred callbacks by default; synchronous Studio update and right-handed
coordinates are per-context options. No-sound mode is a headless smoke path,
not a silent fallback for a missing device.

On any initialization error, Studio is released. A release error is reported
explicitly; the context remains quarantined and cannot be reinitialized until
successful release. `shutdown()` may retry release only while its native
bridge retains ownership and guarantees the pointer remains valid after a
failed call. Destruction fails closed if ownership could not be relinquished.
Successful reinitialization receives a different epoch. `update()` and
`changeDriver()` require initialized state; device selection checks the
actual driver count and surfaces native errors.

## Verification

- `audio.fmod.context-contract` is built without the FMOD SDK. It tests
  invalid input, create/getCore/getVersion/getDriverCount/setDriver,
  no-sound and init failures, rollback, release retry, duplicate init,
  update failure, output-driver changes and restart.
- `audio.fmod.native-context` is added to CTest only when
  `SIR_WITH_FMOD=ON`. It creates a genuine FMOD Studio system in no-sound
  mode, calls update, releases it and repeats initialization/shutdown.
- SDK-disabled CI cannot compile or execute the native bridge. To validate
  that path, install an exact SDK, configure with `-DSIR_WITH_FMOD=ON`
  and `-DSIR_FMOD_SDK_ROOT=/path/to/sdk`, build, and run the FMOD tests.
- Native bank/event ownership, live device-lost callback recovery, FMOD
  memory-domain callbacks, and hardware audio playback remain later waves.


## Wave C — event-instance voice lifecycle

`SIRFMODVoiceBackend` implements `sirAudioBackendOps_t` using a
fixed-capacity native instance table indexed by the portable playing handle's
slot, a sorted registry of pre-resolved native event descriptions, and a
preallocated completion ring. It borrows the initialized `SIRFMODContext`
and must shut down before that context. Registration by event path is an
owner-thread operation performed **after** the event's FMOD bank is loaded;
no string lookup occurs in the playback hot path.

Play creates and starts a Studio event instance and posts STARTED. Natural
stop or a requested stop is detected by owner-thread polling after Studio
update; the event is then released and FINISHED/STOPPED is posted with the
original command sequence. Failed native release preserves the voice slot
for retry. Each accepted voice reserves space for its terminal completion,
preventing silent queue loss. Event descriptions cannot be unregistered
while their instances are active; unregister them before bank unload.

The SDK-only `SIRFMODVoiceNativeApi.cpp` provides the real
`getEvent`, `createInstance`, `start`, `stop`, `getPlaybackState`,
and `release` calls. SDK-free fixture tests cover playback, natural
completion, stop failure/retry, release failure/retry, lookup failures,
resource capacity and completion reservation. Bank ingestion, GRDG
publication, spatial application and parameter bindings remain pending.


## Wave D — bank metadata and sample residency (first slice)

`SIRFMODBankStore` owns the loaded FMOD Studio bank handles and tracks
which logical event IDs it registered against each package. It allocates
bank and binding tables through the caller's Core domain allocator at
initialization and performs no allocations during voice playback.

- `loadBank` uses blocking `Studio::System::loadBankFile` for metadata.
  The master bank and its strings bank should be loaded before dependent
  event banks where the FMOD project requires them.
- `bindEvent` checks that the requested event belongs to the **specific**
  loaded bank, not merely that `Studio::System::getEvent` resolves globally.
  Membership verification enumerates descriptions through
  `Studio::Bank::getEventList` using allocator-accounted temporary scratch.
  Only then does the existing voice registry receive the logical ID.
- `preloadSamples` calls `Studio::Bank::loadSampleData` at most once per
  bank owner. `getSampleState` reports asynchronous loading, readiness or
  error without waiting. Unloading a bank delegates its sample-data
  retirement to FMOD.
- `unloadBank` checks *all* bound events for live instances before unloading.
  An unload failure keeps the bank and registrations for retry. A successful
  unload retires event registrations and releases the bank slot; a failed
  metadata load returning a non-null handle is quarantined for explicit
  cleanup rather than discarded.
- `shutdown` returns `SIR_AUDIO_BUSY` without unloading any bank if a
  tracked instance remains active. A native unload error retains remaining
  handles for retry. Destroying the store with live native bank ownership
  fails closed, consistent with the existing Studio/voice owners.
- Calls are restricted to the Studio owner thread. The context must outlive
  the bank store. Stop/release active instances through the portable audio
  owner first; shut down the voice backend before destroying the Studio
  context. The bank store may unload after the voice backend has shut down.

`audio.fmod.bank-residency` exercises duplicate IDs, package membership,
voice-busy unloads, unload failure and retry, sample preload idempotency,
sample polling failures, allocation rollback, quarantined native load
failure, and destruction order through the injected native-ops fixture.

This slice intentionally does not expose FMOD SDK types in portable
`Source/Audio/` headers. Cooked manifest dependencies, GRDG residency,
audio-streaming policy, device loss and real-bank FMOD SDK execution remain
separate acceptance gates.

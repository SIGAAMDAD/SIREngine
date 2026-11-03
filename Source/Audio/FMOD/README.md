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

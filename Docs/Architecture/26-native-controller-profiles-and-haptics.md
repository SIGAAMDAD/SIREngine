# Native Input Phase 5 — cooked controller profiles and haptic envelopes

## Controller profile cooker

`SIRInputControllerProfiles::saveCooked` and `loadCooked` read/write a 20-byte header with `SICP` magic, version 1, count, total bytes and FNV-1a payload checksum. Each variable-size profile record contains bus/vendor/product, replacement flags, button/axis counts, button native-to-logical map pairs and axis native-to-logical + inversion triples. All numbers are explicitly little-endian; reserved fields are zero and checked. Loading validates lengths, checksum, enumerations, mapping uniqueness and duplicate device keys into a temporary fixed-capacity registry before swapping. Maximum asset size is `SIR_INPUT_COOKED_PROFILE_MAX_BYTES`.

Linux evdev imports cooked profiles only before discovery and merges them transactionally with any already-registered profiles. `SIR_INPUT_CONTROLLER_PROFILE_DB` may designate a cooked binary file for the X11 demonstration launcher. The file is read once, bounded, and never parsed or allocated on the input hot path. The existing exact-bus and generic-bus precedence is preserved. Windows GameInput/XInput retain their native normalized maps and can adopt the common cooker when vendor/HID layouts become available.

## Haptic mixer

`SIRInputHaptics` provides 64 fixed effect slots, generation-tagged cancellation handles, up to 60-second request duration, per-device attack/sustain/release motor envelopes and highest-priority per-motor mixing. Durations use engine monotonic time, not simulation tick counts. The input service samples haptic output once per collecting tick and sends only changed values, or zero at expiration, pause, device disconnect and shutdown. Failed native writes remain retryable until the next collecting tick. Pause on window focus loss stops output.

Backend capability flags distinguish ordinary rumble, trigger rumble, LED, adaptive triggers and HD haptics. The current implementations report only actual supported capabilities. XInput and Linux evdev support the ordinary low/high rumble path where their native device reports it. GameInput v1 queries `IGameInputDevice::GetDeviceInfo()->supportedRumbleMotors` and maps low/high and left/right trigger motors to the common API. Low/high and trigger commands retain each other's last motor values when either is updated.

The compositor accepts no arbitrary hardware patterns, arbitrary vendor output reports or opaque commands. LED, adaptive triggers, HD haptics, device gyro/touch and advanced controller families remain **unimplemented hardware backends**, with capability bits reserved for later integration.

## Acceptance

The `core.platform.input.ui-haptics-profiles` unit suite covers cooked assets round-trip, rejection of corrupt/truncated assets, transactional Linux import, haptic envelope priority/cancel/release, and per-device lifecycle/pause. Windows and Linux native C++17 and formatting gates must be run against the same source SHA; USB/Bluetooth, vendor-specific devices and physical IME/rumble output require hardware-in-loop testing.

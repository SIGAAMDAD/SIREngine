# SIREngine input Phase 2 — logical action resolution and profiles

**Branch:** `feature/native-input-foundation`  
**Owner:** `Source/Core/Platform/Input/SIRInputActions`

## Runtime contract

`SIRInputService` owns one `SIRInputActions` instance. Its `publish()` evaluates ordered canonical input events immediately after frame-state publication. Consumers query logical actions through `input.actions().query(actionId, user, &state)`. State reports held/pressed/released, signed normalized value and transition count for the most recently published simulation tick. No OS SDK or SDL types cross this boundary.

Bounded tables: 128 action/user entries, 16 context/user entries, 256 bindings. Repeated runtime evaluation never allocates, and physical state is keyed by generation-checked device slot. Unpaired devices currently route to user 0; paired devices use the published registry user assignment.

A binding identifies action ID, context ID, user, physical device class, signal, physical control number, optional same-device keyboard/button chord, deadzone, signed Q15 scale, and two flags: `CONSUME` or `TOGGLE`. Multiple bindings may refer to the same action across devices or contexts. Higher-priority consuming contexts suppress lower-priority bindings to the same active physical control. Context enable/disable and rebind operations invalidate evaluated state at the next tick. Binding conflicts within the same user/context/device-class/signal/control/chord tuple are rejected. Binding removal compacts the table.

## Cooked/profile format

`saveProfile` and `loadProfile` use an explicitly versioned little-endian stream, not raw C++ object bytes or pointer addresses. Header is 24 bytes: `SIRA` magic, version 1, 24-byte header size, context/action/binding counts and FNV-1a32 payload checksum. Context records are 16 bytes; action records 8 bytes; binding records 28 bytes. Numeric fields, reserved padding, counts, total length, unique IDs, binding conflicts and checksums are verified during deserialization. Loading is transactional: all records are validated into an independent temporary action map before the active settings are replaced. A failed load leaves existing settings intact.

Recording/replay timestamps, profile origin signatures, editor UI for binding authoring, user-directed hardware capture, advanced input calibration and long-term migration across profile schema versions remain separate subsystems.

## Acceptance

`core.platform.input.actions` exercises context priority and consumption, held/pressed/released transitions, conflicting rebind rejection, remapping, same-tick short taps, modifier chords, toggles, controller axes with deadzones/inversion, device disconnect release, deterministic profile round-trip, rejected corrupt/truncated profiles and the no-input headless application lifecycle. The existing input stress suite remains a regression gate.

Native hardware and presentation latency tests are independent of action-map unit tests.

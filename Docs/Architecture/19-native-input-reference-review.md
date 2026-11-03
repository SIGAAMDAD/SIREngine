# 19 — Native input: source-grounded architecture review

**Scope:** Native input under `Source/Core/Platform/`, based on the
`feature/native-input-foundation` branch at `39ce47158773d3e7332f0091e1034ea0c6471575`.
This is an architecture and implementation-status record, not hardware acceptance.

## Actual reference implementations inspected

| Reference archive | File and relevant responsibilities | SIREngine decision |
| --- | --- | --- |
| Supplied UE5 `Runtime.zip` | `Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`: owned Windows message dispatch, device/input interface wiring | Window owner pumps native messages and invokes collectors; input must not create a competing Win32 message loop |
| Supplied UE5 `Runtime.zip` | `Runtime/InputDevice/Public/IInputDevice.h`: explicit device tick/event output, haptics, declared thread affinity | Define collector ownership and thread-affinity contract before async backend work |
| Supplied UE5 `Runtime.zip` | `Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformInputDeviceMapper.h` and `InputDeviceRegistry.h`: logical user-to-device mapping and independently registered hardware descriptors | Separate hardware identity, engine device handles, and user assignment; the current `nNativeId` key is insufficient for multi-transport deduplication |
| Supplied Source Engine `source-engine-master.zip` | `inputsystem/inputsystem.h` and `inputsystem/inputsystem.cpp`: `INPUT_STATE_QUEUED`, `INPUT_STATE_CURRENT`, `PollInputState`, button press/release ticks, analog state and buffered events | Publish stable tick state separately from the owner-mutated live state; retain ordered transition events |
| Supplied Source Engine `source-engine-master.zip` | `public/inputsystem/iinputsystem.h`: device sampling, input-state queries, window binding, rumble and focus reset | Keep a small engine-owned consumer contract; native collectors remain behind it |
| Microsoft GameInput documentation | `GetCurrentReading` versus `GetNextReading` and per-device history | Prefer historical reading traversal for devices that must preserve short transitions; define explicit history-gap recovery |

Source archive inspection is architectural only; SIREngine declarations and implementation are independently authored. The supplied Source Engine version contains SDL controller integration, which is not a candidate for SIREngine's Windows/Linux runtime dependencies.

## Current implementation and limitations

- `SIRInputSystem` owns a fixed-capacity event queue, generation-checked slots, live device state, analog readings and press/release edges. Collectors run on its owner thread. A new `publishTick`/`getPublishedState` API takes a bounded, allocation-free copy of the 32 device slots after native sampling. `beginTick` resets live transient edges for the next tick without mutating the previous publication. Publication is strictly increasing by tick number and rejects queue-overflow state. This is a single-thread contract, **not** a lock-free multi-reader publication protocol.
- `SIRWindowsRawInput` consumes Win32 messages forwarded by an owning window; `SIRWindowsXInput` covers only XInput-compatible controller state, vibration and reconnect. The current `getState` validity probe in the XInput loop copies an entire device state; use `isDeviceActive` to check handle generation without that copy.
- Linux evdev has frame batching on `SYN_REPORT`, `SYN_DROPPED` resynchronization, profile registry, inotify hotplug, and `EV_FF` output. Linux X11 accepts owner-forwarded core XEvents. Neither is currently wired to a production SIREngine window/event loop.
- The engine currently lacks a central physical-device arbiter. GameInput, XInput, and HID can expose the same controller. Do not enable a second Windows controller transport until exact ownership and de-duplication behavior is specified and tested.
- `SIRInputControllerProfiles` is an in-memory, fixed-capacity mapping registry, not a cooked/versioned device mapping database. No authored profile validation/serialization pipeline has shipped.
- X11 core motion reports cursor movement, not XI2 unaccelerated raw motion. Wayland focus/text, compositor pointer lock, IME, and non-Windows/Linux SDL3 backends are pending.
- `SIRInputSystem::resynchronizeDevice` resets state on queue loss and emits a resync marker; it does not itself query hardware or establish synchronized cross-thread visibility. Backends are responsible for querying authoritative state. Focus loss and replay consumers must explicitly process resync events until full release-edge and recording semantics are implemented.

## Hard architectural boundaries for subsequent work

1. **Single event owner:** OS window/display integration controls message pumping and focus; each controller transport submits normalized records to the input owner. No per-event job scheduling, hidden heap allocations, or simultaneous writers to `SIRInputSystem`.
2. **Device arbitration before new transports:** map physical identity/transport/interface metadata to one engine device handle, with generation and source provenance. Player/user binding is a separate layer. Verify composite HID/evdev nodes and GameInput/XInput duplicates under actual hardware.
3. **Tick ownership:** `beginTick` → collect/drain native input → `publishTick` → read published tick. The current API only guarantees stability on the single owner thread. Render/physics cross-thread reads require an explicit double-buffer publication/reclamation protocol and real concurrency tests before being advertised as thread-safe.
4. **Backend capabilities:** input, haptics, sensors, LEDs and special controls are per-device capabilities. Failed or unsupported operations return status; avoid synthetic success.
5. **Buffered hardware readings:** use API-provided ordered history where available (for example GameInput `GetNextReading`). Latest-state XInput polling is a compatibility backend and cannot guarantee preservation of transitions occurring entirely between polls.
6. **Input delivery and action mapping:** preserve physical input events and pressed/released transitions independently of action bindings. Gameplay, UI and editor consume stable tick data through future context/priority mapping, rather than each subsystem polling OS handles.

## Measured and missing acceptance

- Local Linux GNU C++17 tests and Clang ASan/UBSan tests exercise the input core and existing X11/evdev collector source. Windows XInput logic still needs a native Windows SDK build and physical-device validation.
- On the Linux x86-64 local compiler, `SIRInputSystem` occupies 84,016 bytes after adding the fixed published-slot array; one publication copies 32 × 672 bytes of device-slot state. These values are compiler/ABI specific and are *not* a CPU benchmark.
- Remaining gates: integrated CMake/FASTBuild matrix, repository-wide pinned formatting/AST checks, native Win32 link+runtime, XI2/Wayland device focus and IME, GameInput history and deduplication, hardware mapping and haptics matrix, and deterministic replay acceptance.

## External API references

- [Unreal IInputDevice](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/InputDevice/IInputDevice)
- [Unreal IPlatformInputDeviceMapper](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/ApplicationCore/IPlatformInputDeviceMapper)
- [Microsoft GameInput readings](https://learn.microsoft.com/en-us/xbox/gdk/docs/features/common/input/overviews/input-readings)
- [Microsoft GetNextReading](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinput/methods/igameinput_getnextreading)

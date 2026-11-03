# SIREngine native input — platform ownership and frame-state replacement

**Revision:** 2026-11-03. **Target branch:** `feature/native-input-foundation`.

## Source bases consulted

- Unreal Engine 5 Runtime archive supplied by the project: `Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformInputDeviceMapper.h`, `InputDeviceRegistry.h`, `GenericApplicationMessageHandler.h`, `IInputInterface.h`, plus `Private/GenericPlatform/InputDeviceRegistry.cpp`.
- Source Engine archive supplied by the project: `public/inputsystem/iinputsystem.h`, `inputsystem/inputsystem.h`, `inputsystem/inputsystem.cpp`.

UE5 is the reference for **per-device identity and independent user assignment**, native device ownership, and platform event dispatch. Source is the reference for **polled frames, button edges, analog state, event data and tick boundaries**. The SIREngine implementation is original C++17 using fixed-capacity first-party data structures and the project's ownership/format policy. These references are design evidence; no third-party code is embedded.

## Ownership and source layout

`Source/Core/Platform/Input` owns five distinct contracts:

1. `SIRInputTypes.h`: fixed-size normalized control identity and event wire types.
2. `SIRInputDeviceRegistry`: authoritative native device table with backend ID, physical/native identities, generation-tagged engine device IDs and user pairing. A physical identity can only have one authoritative backend per device class. Native APIs are never accessible through this public type.
3. `SIRInputFrameState`: the single-writer frame accumulator; it batches transitions atomically, preserves press/release pairs, records analog and pointer changes, and publishes a stable tick snapshot. `frame()` is a borrowed immutable pointer valid until the next published frame or shutdown.
4. `SIRInputBackend`: first-party native I/O owner abstraction with explicit `sample`, optional haptic output, and `stop`. No backend owns game state.
5. `SIRInputService`: one orchestrator combining registry and frame state; it verifies emitting backend ownership, routes haptics, enforces collection/publish boundaries and owns backend registration/shutdown order.

Native paths:
- Windows: `Input/Windows/SIRWindowsRawBackend`, `SIRWindowsXInputBackend`.
- Linux: `Input/Linux/SIRLinuxX11Backend`, `SIRLinuxEvdevBackend`.
- Wayland, GameInput, extended Windows/Linux HID, text/IME, sensor APIs and SDL3 ports are subsequent native backend work. SDL3 is reserved for non-Windows/Linux targets.

**Early-development source replacement:** The previous `SIRInputSystem`, `SIRInputCoordinator`, `SIRInputControllerProfiles`, and their native collector APIs are deleted. There is no source or binary bridge, no forwarding shim, and no old-name aliases. Tests target only the new API. CMake registers only new translation units.

## Tick and device lifecycle

- The application creates `SIRInputService`, native backends and an OS window; attaches each backend exactly once before the first input tick.
- `begin(tickId)` opens one collection interval, clears per-tick edges/deltas and retains prior held state.
- Window owners dispatch events to their backend during collection. `sample()` polls the native gamepad/evdev owners. All backend callbacks remain on the input-owner thread.
- `publish()` atomically copies the completed event list and current device state to the next published slot. A second `begin()` during collection and non-increasing tick IDs are rejected.
- `frame()` exposes events and state for that tick without mutation. The next publish invalidates previous borrowed frame lifetime after it is overwritten in a later publication.
- The registry alone connects and disconnects a device. Any reconnect obtains a changed generation; stale handles and cross-backend writes are rejected. Connected descriptors are embedded in the frame at publication.
- Output requests are routed exclusively to the backend owning that device. Haptic support and request completeness must be confirmed by real hardware tests.
- `shutdown()` stops native backends in reverse attach order before invalidating registration and frame storage.

The event buffer is fixed at 2048, device slots at 32, and axes at 16. A multi-control batch preflights the entire transition count before mutation. Overflow blocks publication until the owner explicitly runs `recover()`, which clears the incomplete stream and publishes `SIR_INPUT_RESYNC` notices for all connected devices. A device-specific native backend must subsequently re-query authoritative state. Overflow cannot be represented as successful complete input delivery.

## Current acceptance and limitations

Run focused C++17 `SIRInputTests` for device ownership, physical identity collision, generation reuse, user pairing, ordered same-tick edges, tick publication, atomic overflow and cross-backend rejection. Test optional X11 on/off builds. Use repository clang-format 18.1.8.

**Not certified by isolated tests:** native Windows SDK build; real Windows message loop dispatch; Wayland seat/keymap/relative pointer/text protocols; GameInput/HID arbitration; extended controller maps; physical rumble and high-rate hardware benchmarks; full SIREngine CMake/FASTBuild tests. These are separate acceptance gates, not silently delegated to this replacement.

## Before merge

Require a source-tree audit with zero references to superseded input declarations, source-level compiler tests for Windows/Linux native TUs, repository pinned style test, complete CI matrix, and platform/window service integration tests. The inspected branch has no consumers of the retired interface; the launcher does not yet own a window or call this subsystem.

## First-party native controller profiles and state recovery

The Linux evdev backend accepts validated, fixed-capacity controller profiles registered before discovery. Selection uses the native `EVIOCGID` bus/vendor/product triple, with exact bus matches ahead of transport-independent matches. Each device copies its selected mapping at open. Profiles can replace native button and axis assignments, while the generic Linux gamepad layout remains the fallback for unrecognized hardware. Runtime evdev samples and authoritative state re-query use the identical selected mapping. This is an in-memory registry; authored/cooked profile serialization remains outstanding.

A frame that overflows its bounded event buffer must be recovered before publishing or beginning another tick. Explicit per-device resynchronization increments the frame resynchronization counter; the backend retains responsibility for re-reading hardware state.

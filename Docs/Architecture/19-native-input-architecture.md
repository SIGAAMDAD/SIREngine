# 19 — Native input ownership and polled-state architecture

**Related preexisting review:** [19-native-input-reference-review.md](19-native-input-reference-review.md). This document adds the ownership/coordinator contracts to that snapshot review.

**Status:** Revised ownership/state architecture implemented as a native foundation on `feature/native-input-foundation`; full window integration, GameInput, Wayland, HID, mapping database cook, and hardware acceptance remain pending.

## Source-grounded reference decisions

This design deliberately borrows **contracts and responsibilities**, not code, from the engine source snapshots supplied to the engineering workspace:

| Reference | Examined source | SIREngine decision |
|---|---|---|
| UE5 ApplicationCore / InputDevice | `Runtime/InputDevice/Public/IInputDevice.h` | Backend interface with explicit sampling, native shutdown and device ownership. No per-event allocations or platform objects in public runtime state. |
| UE5 platform input mapping | `Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformInputDeviceMapper.h` and `InputDeviceRegistry.h` | Separate backend ownership, engine device identity and user pairing. A disconnected generation is not the same device handle after reuse. |
| UE5 Windows / Linux application | `Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`, `.../Linux/LinuxApplication.cpp` | A window/application loop owns OS event dispatch; input adapters do not create competing loops. |
| Source 2017 | `public/inputsystem/iinputsystem.h`, `inputsystem/inputsystem.h`, `inputsystem/inputsystem.cpp` | Live queued events and mutable device states are *published* as stable per-tick current snapshots. Button edges and analog data are accessible without reinterpreting raw packets at gameplay call sites. |

References are independently implemented, not copied or licensed into SIREngine. Exact platform behavior is defined by OS APIs and validated against hardware rather than inferred from older engine implementations.

## Ownership graph

```
Game/editor/main window event loop (input owner thread)
 └─ SIRInputCoordinator (registered backend lifecycle, user/device routes)
     ├─ SIRInputBackend (abstract, non-owning registration; one owner per device)
     │   ├─ Win32 Raw Input: external WndProc-driven keyboard/mouse
     │   ├─ XInput: polling gamepads and rumble
     │   ├─ Linux X11: external XEvent-driven desktop input
     │   └─ Linux evdev: native controller polling, profiles and EV_FF
     └─ SIRInputSystem (canonical live state and bounded event queue)
         └─ publishTick: immutable-by-contract tick state + event batch
             └─ gameplay/editor query readers on same owner thread
```

The application creates `SIRInputSystem`, native backend objects, and `SIRInputCoordinator` in a lifetime-safe order. The coordinator *borrows* backend objects, owns registration and shutdown ordering, and must call `shutdown()` **while all backends still exist**. It must not outlive the shared `SIRInputSystem`. Backend destructors also release native handles; repeated shutdown must remain safe. Native handles, HID packet descriptors, `Display*`, `HWND` and SDL types remain inside the native collectors.

### Input thread contract

All operations shown above currently run on **one input owner thread**, including native callbacks, device discovery, polling, publication, and user assignment. `publishTick` is immutable to that thread until the next publication. This is not an atomic multi-reader snapshot and must not be exposed directly to independently scheduled render/physics threads; the later job/GRDG integration requires an explicitly tested lease or double/triple-buffer publication protocol.

A future background HID worker may submit through a bounded synchronized ingress. It must **not** call `SIRInputSystem::push*` concurrently. One thread owns native window event pumping per window/seat. The framework's global event bus is not invoked for each physical motion sample.

## Lifecycle and frame phases

1. Create SIRInputSystem and platform collectors. Register collectors with SIRInputCoordinator **before input begins**. Registration refuses duplicate backend implementations and concurrent desktop collectors.
2. Initialize native collectors/window services, attach surface and controller endpoints, then call `beginTick(nTick)` before dispatching that tick's OS messages. `beginTick` clears transient live edges while retaining held state.
3. The window owner forwards Win32/X11 events to their event-driven backends. `publishTick()` pumps polling backends (XInput/evdev), validates device ownership and physical-identity conflicts, then publishes the complete tick to the shared input state.
4. Gameplay/editor consumers use `getPublishedState`, `getPublishedEventCount` and `copyPublishedEvents`. The publication **drains** the old mutable event queue into that tick's ordered event batch; legacy `pollEvent` callers can continue to use the uncommitted kernel but must not consume events while a coordinator tick is being collected.
5. Pair connected device handles to users through `assignUser`. The default is `SIR_INPUT_UNPAIRED_USER`. A new generation on the same slot resets pairing, preventing stale controller ownership.
6. Haptics route only through the registered owner. Typed `setVibration` returns unsupported when a backend cannot provide the requested effect lifetime; XInput does not claim automatic timed shutdown.
7. Shutdown in reverse backend registration order, before destroying the coordinator/backend objects, then invalidate all canonical device generations.

A failed `publishTick` is observable through `getLastResult()`. Ownership reconciliation is **transactional**: a complete candidate table is checked before committing any routes; simultaneous identity changes cannot read stale keys from the prior tick, and conflicts do not mutate the previously published route map. Input overflow rejects publication until the owner performs an explicit resync; lost edges are never represented as successfully published. Do not treat an abandoned/failed tick as accepted simulation input.

## Device registry and physical identity

`SIRInputCoordinator` maintains one fixed slot-indexed route per canonical `sirInputDeviceHandle_t`. A route contains the native **owner kind**, an optional stable physical key and the assigned engine user. All handles are generation checked. Backends answer `ownsDevice` for live handles. Zero owners, two owners or two routes with the same verified nonzero physical key reject tick publication.

A zero physical key means **unverified/unknown**, not a unique controller. Existing evdev/XInput collectors do not yet supply a reliable cross-transport identity and therefore cannot resolve GameInput+XInput or composite hidraw+evdev duplicates by themselves. **The required next integration must add a verified OS physical-device arbiter before enabling overlapping gamepad collectors**. Do not invent uniqueness by hashing a vendor/product ID or a transient XInput slot. The exact key may be a verified OS device-container/path identity scoped to a boot/session and generation.

The actual public routes are calculated from backend ownership; no game action may write `sirInputDeviceState_t` directly. Backend registration is fixed-size, duplicates fail closed, and no backend may reassign another backend's active device.

## Memory/data layout

The input owner keeps fixed-capacity arrays for 32 devices, 8 backends, 8 users, 1024 queued events and 1024 published events. There is no per-event allocation in the kernel/coordinator. Device states use bitset buttons, pressed/released bits, normalized axes, motion deltas and generation-tagged slots. `SIRInputSystem` copies live state and drains an ordered event batch during publication; this favors correct input-state semantics now, pending per-dirty-slot optimization with benchmark evidence.

Immutable published snapshots remain valid until the next `publishTick()` *on the owner thread*. Do not distribute references across frames or threads. `copyPublishedEvents` accepts caller-owned output storage with explicit capacity. C++17, SIREngine global namespace, owned headers, memory ownership and clang-format 18.1.8 apply.

Memory-attribution integration with SIRMemoryManager can follow the same design once its domain and thread-lifecycle dependencies are established; this slice's bounded built-in storage currently has no dynamic allocations to route.

## Native backend scope / backlog

| Backend | Current input architecture | Remaining work |
|---|---|---|
| Windows Win32 Raw Input | Event-driven keyboard/mouse backend adapter | Native window loop wiring, text/IME, Raw Input buffered reads, high-rate tests |
| Windows XInput | Polling backend, gamepad standardization, vibration | GameInput/HID physical-device arbitration and real hardware smoke tests |
| Linux X11 | Event-driven window input backend adapter | XInput2 relative/raw motion, XKB/text input, real desktop app lifecycle |
| Linux evdev | Polling backend, controller mapping, hotplug, EV_FF | Composite device dedup, udev session rights, advanced HID, Bluetooth identity |
| Linux Wayland | Interface kind reserved, not yet implemented | wl_seat/keyboard/pointer, xkbcommon, relative-pointer/constraints, text-input protocol |
| SDL3 other OS | Interface kind reserved, not yet implemented | Only after that operating system's platform/window foundations are enabled |

Windows and Linux shipping builds must not depend on SDL3. The backend interface itself does not load SDL; build-system target and link inspection remain required gates.

## Mandatory negative acceptance

- **Duplicate backend:** two instances of the same backend kind are rejected; two simultaneous desktop backends are rejected.
- **Duplicate native owner:** the same active handle claimed by two backends rejects a tick without partially updating the route registry.
- **Duplicate verified identity:** two handles with the same nonzero physical key reject publication; unknown identities remain explicitly unknown.
- **Reconnect/stale generation:** releasing and reallocating a slot invalidates the old route and user pairing.
- **Source-style tick state:** press/release within one tick remains visible both as ordered events and as pressed/released edges; subsequent live mutations do not alter the previous published state.
- **Ordered event views:** publication moves queued events to a stable batch and frees queue capacity; a second tick must not replay the earlier event batch.
- **Overflow:** full-queue publication fails; recovery must identify missing transitions and resynchronize explicitly.
- **Output capability:** vibration reaches only the recorded owning backend and fails for unsupported timed effects.
- **Lifecycle:** stop shuts down collectors before invalidating canonical input handles, and disallows new ticks/registrations.
- **Platform:** Linux X11-enabled and X11-disabled C++17 tests, GCC/Clang ASan/UBSan, Windows SDK build, physical hardware, formatting, and full CMake/CTest CI. Synthetic tests alone do not certify OS-specific operation.

## Integration gates

1. Merge the common ownership + snapshot layer and native adapter interface conversion (this slice). Keep old low-level device state APIs available for backend compatibility.
2. Establish actual engine window ownership and event loop ordering, then route Win32/X11 through the coordinator, not independent application polling.
3. Implement GameInput/HID device arbitration with identity verification; then make standard/advanced output authoritative in one backend.
4. Add Wayland, text/IME and XInput2 raw motion behind the same `SIRInputBackend` boundary.
5. Cook mapping profiles, add player action contexts and scripting, and measure p50/p95/p99 latency, CPU cost and resident storage with identical physical workloads.
6. Remove legacy direct gameplay polling of `SIRInputSystem` only after callers migrate to the per-tick published interface. Complete full CI, Windows/Linux native runtime tests and device matrices before merging to develop.

**Document authority:** source design and reference-code inspection, with tested *common* implementation. No claim of full engine, native Windows SDK or hardware validation. The reference implementation may be evolving; verify actual source SHAs and tests before merging.

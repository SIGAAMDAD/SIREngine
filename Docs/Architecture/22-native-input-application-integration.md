# Native input application integration — Phase 1

**Owner:** `Source/Core/SIRApplication`, `Source/Core/Platform/Input/SIRInputWindow` and first-party platform backends.

## Lifecycle

The application can attach exactly one borrowed `SIRInputService` and a borrowed window-event pump before initialization. Its tick performs:

1. `begin(nextTick)` while the application is ready.
2. Dispatch native window events on the same input owner thread through a caller-supplied pump.
3. Sample attached polling backends.
4. Publish an immutable input snapshot, recovering explicit queue overflow when needed.
5. Advance the application tick once a snapshot has been published.

A pump returning false requests loop termination. Input publication still runs so pending disconnects and releases are observable by the caller. The application owns input shutdown and stops backend producers before clearing its event registry. The backend and native window objects themselves remain caller-owned and must outlive application shutdown.

The launcher retains its original headless one-tick behavior by default. `SIREngine --input-window` exercises the platform-native desktop input window when available; Linux builds require X11 support and a usable X11 display, while Windows builds use the Win32 Raw Input window owner. No SDL3 links are introduced.

## Ownership and event handling

`SIRInputWindow` creates and owns the native window, sends Win32 Raw Input or X11 events to its already-attached backend, and exposes no native types to callers. On Windows, Win32 raw input is registered once for its process-scoped device classes, the dispatched WM_INPUT report is decoded before the default cleanup handler, and focus loss is deferred to the collecting interval. On Linux, the X11 adapter connects its per-window keyboard and pointer only after the tick begins, then dispatches focus and control events. This phase does not implement Wayland, XInput2 raw mouse, UI text composition, full window-manager lifecycle, controller arbitration, presentation scheduling or game-action mapping.

`SIRInputService::loseBackendState` performs a bounded scan of the existing device registry and publishes per-device RESYNC transitions for the selected backend. It uses the same owner-thread collection contract as all input events.

## Verification requirements

The new `core.application.input-lifecycle` CTest covers the mock backend on headless Windows/Linux builds: one-time attachment, rejection of late/duplicate attachment, ordered pump/sample/publish, input frame edge preservation, failed pump return after publication, backend stop exactly once, shutdown and legacy headless operation.

Run repository-pinned formatting, Linux GCC, Windows MSVC, focused Clang ASan/UBSan and the CTest suite on the **same source SHA**. A native window hardware smoke with focus changes and 1–8 kHz mouse input remains a separate acceptance gate; the feature PR should remain draft until that evidence exists.

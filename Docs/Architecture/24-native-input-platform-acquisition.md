# Native input phase 3 — platform acquisition integration

**Current scope:** Windows native controller runtime selection; Win32 Raw Input arrival reporting; Linux evdev controller discovery and physical identity; optional XInput2 raw motion.

## Windows

The native input window launcher activates the existing Raw Input backend for keyboard/mouse and selects **one** first-party controller collector: GameInput v1 when its SDK was compiled and the runtime initializes, otherwise XInput. One active controller API prevents duplicate Xbox-compatible gamepad delivery until full per-device GameInput/XInput/HID physical arbitration is implemented. It does not yet expose legacy-only pads alongside GameInput pads when GameInput is available.

Win32 Raw Input now registers native arrival notifications through `GetRawInputDeviceInfoW(RIDI_DEVICEINFO)`, registering keyboard/mouse device identities before their first key/mouse packet. Removal retires their engine handles.

The existing GameInput history, generation-aware registry, rumble and XInput sampling remain unchanged. Vendor HID/SetupAPI feature arbitration and persistent controller identity are not implemented in this slice.

## Linux evdev

The launcher attaches the existing evdev gamepad collector separately from the X11 desktop collector. The first collecting tick performs one initial device discovery scan, then the existing inotify watch handles node arrivals/removals. Normal user permissions remain authoritative. No direct /dev/input keyboard or mouse capture is used.

A new bounded FNV-1a physical-key function collapses evdev composite interface suffixes (`/inputN`) while preserving the bus location and VID/PID. This allows the existing device registry to reject multiple event nodes belonging to one gamepad. Devices with no reliable physical path retain native per-event-node IDs; this cannot ensure cross-API deduplication for every controller.

## X11 XInput2

When X11 Xi headers and linkable library are both available, `SIRLinuxX11Backend` negotiates XInput2 at runtime and selects `XI_RawMotion` on the root window, maintaining fractional delta carry. The window owner forwards GenericEvents to that backend. Legacy X11 pointer motion is suppressed when the raw path is active. Pointer input is applied only while the input window has focus. If XI2 support is not available, existing focused X11 pointer motion remains active.

## Acceptance and remaining platform scope

- Deterministic physical-key tests cover composite device identity, separate USB ports and absent physical paths.
- Linux/Windows C++17, format and native CTest must pass at the same source SHA.
- Device/hardware testing is not yet performed for this patch.

**Still required before phase 3 can be called complete:** a native Wayland seat/keyboard/pointer/relative-pointer/constraint/IME implementation and its window owner; XInput2 focus/capture hardware verification; libudev/hidraw interface grouping, vendor-specific report decoding, USB/Bluetooth resumption; Windows per-physical GameInput+XInput+HID arbitration; controller hotplug/rumble hardware-in-loop; native Wayland/X11/Windows message loop acceptance under real desktop sessions. The fallback controller selection is an interim correctness policy, not full cross-API device arbitration.

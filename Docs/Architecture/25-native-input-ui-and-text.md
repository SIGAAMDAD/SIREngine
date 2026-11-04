# Native Input Phase 4 — UI, scripting boundary and text/IME

## Engine-owned interfaces

`SIRInputService::text()` exposes `SIRInputText`: a fixed 256-codepoint committed-text queue, 128-codepoint preedit buffer, caret/selection, revision, focus/enabled state, invalid encoding rejection and overflow indicator. `begin(tick)` clears transient committed text, `publish()` freezes the snapshot, and `shutdown()` resets it. Committed text never shares the physical keyboard state or consumes action edges.

`SIRInputUIDispatch` is a per-subscriber, no-allocation dispatch cursor using caller-owned POD `sirInputUIConsumer_t` callbacks: requested per-user action state, text Unicode scalar values, composition snapshots, focus changes, and native pointer events. It guards one dispatch per published tick. Editor widgets and future scripting bindings can use this interface without exposing X11, Win32 or SDK types.

`SIRInputWindow` exposes `setTextEnabled`, `setCursorMode` (free, hidden, captured), `cursorMode` and `focused`. Win32 handles `WM_CHAR`, `WM_UNICHAR`, UTF-16 surrogate pairs, `WM_IME_COMPOSITION` preedit through Imm32, composition cancellation, focus, mouse clipping and a hidden cursor. X11 uses `XOpenIM`/`XCreateIC`, `Xutf8LookupString` committed text, IME event filtering, focus, native pointer grab and an invisible cursor. Core text UTF-8/UTF-16 decoding and preedit replacement are fully deterministic and reject malformed data.

All native message delivery occurs on the input owner thread between `begin` and `publish`; no per-frame allocation is introduced to the core text and dispatch types.

## Limits and later consumers

The repository currently contains **no migrated editor or scripting runtime**. The binding/consumer ABI is implemented, but there is no actual editor widget, GDScript, SUIL or other scripting VM caller to migrate here. X11 XIM currently uses the server-managed `XIMPreeditNothing` style and forwards committed text; embedded preedit callbacks/selection require the editor's text widget integration. Wayland native text-input/relative-pointer/seat remains part of unfinished phase 3; Linux X11 and Windows are the native implementations exercised by this patch.

## Tests

`core.platform.input.ui-haptics-profiles` covers Unicode UTF-8/UTF-16, invalid sequences, queue overflow, preedit, focus cancellation, per-tick UI callbacks, pointer and action routing. Native IME and cursor behavior require focused hardware/display tests on Windows/X11 and eventual Wayland.

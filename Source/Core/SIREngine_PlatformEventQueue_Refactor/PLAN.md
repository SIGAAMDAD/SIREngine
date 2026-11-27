# SIREngine event queue ownership: light refactor

## Authority and scope

Based on the supplied `Linux.zip` and `Shared.zip` plus the previously delivered
Linux input-queue migration. This is a local artifact, not a change to remote `develop`.

## Implemented architecture

```
Engine/platform host (owner) : SIRSPSCEventRing<SIREventData>
     |                                                |
     v                                                v
SIRNativeWindowDriver (borrowed m_EventQueue&)   SIRNativeInputDriver (borrowed m_EventQueue&)
     |                                                | 
     v                                                +-- X11 input / Wayland input
X11 window driver                                    |
                                                     +-- Evdev gamepad (same borrowed queue)
                                              \
                                         one engine consumer -> window/input/application
```

The two platform-agnostic base classes store a `protected` `SIRSPSCEventRing<SIREventData>&`;
constructor injection requires non-null references and guarantees no copy of the queue.
X11 window and desktop input implementations initialize their base classes, and neither
shadows its base `m_EventQueue` member. Desktop input drivers pass `m_EventQueue` to
embedded evdev. Native drivers publish `SIREventData` via `tryEnqueue()`, not
`SIREvent<SIREventData>::publish()` or `SIRInputService`.

Evdev no longer contains `SIRInputDeviceRegistry` or mutates input-system registration
state. The native driver keeps only local fixed-capacity slot/generation bookkeeping,
preserving gamepad event IDs and rejecting duplicate physical/native devices. Read-only
controller profile mapping and normalization helpers are intentionally unchanged.

## Required host wiring (host was not in either upload)

- Construct one queue in the platform-agnostic host; keep it alive until both drivers
  have been stopped and destroyed.
- Construct selected window + input drivers with the exact same queue reference.
- Pump both drivers from the *same producer thread*; this queue is SPSC, not MPSC.
- Consume via `tryDequeue` on the sole consumer thread and update engine systems.
- Handle `droppedEventCount()` and queue-full return codes with input-state resync.
- Configure X11 core vs XI2 input authority to avoid duplicate keyboard/mouse events.

## Excluded / blocked by missing sources

- No platform-agnostic composition root/dispatcher was included; consumer wiring
  is therefore not implemented or claimed complete.
- `SIRNativeWindowWayland.h` remains a TODO skeleton in the supplied source.
- The supplied `Shared/Events/SIREventTypes.h` and `Shared/Window/SIREventTypes.h`
  have conflicting `SIREventType` definitions and the same include guard; the
  engine-wide event type consolidation needs the complete Core/Events baseline.
- Full X11 window translation-unit compilation is already blocked by unrelated
  enum-to-integer assignments and `m_NativeView` references absent from the supplied
  platform-agnostic base; the queue-reference patch does not change those statements.

## Validation requirements

1. GCC and Clang C++17 header and constructor compile checks.
2. GCC and Clang X11 input and evdev syntax checks with explicitly labelled
   compatibility stubs for missing Core components.
3. Clang AST confirms `m_EventQueue` is a reference in *both* shared base classes.
4. Runtime probe constructs both shared base types with one queue and verifies identity.
5. Full build, runtime tests on X11 and Wayland, real SPSC ring behavior, and consumer
   routing remain unverified until integrated with the missing engine sources.

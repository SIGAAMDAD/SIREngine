# Native input phase 6 — logical action tick recording and replay

**Owner:** `Source/Core/Platform/Input/SIRInputReplay.{h,cpp}`  
**Mode:** deterministic logical-action playback, independent of native hardware state

## Contract

`SIRInputReplayTape` writes and reads exact **post-action-mapping** tick state. After `SIRInputService::publish()`, the owner thread calls `recordReplayTick(tape, keys, count)` with a fixed list of explicitly identified logical actions and users. Each recorded state includes action ID, user ID, signed Q15 action value, total transition count, pressed, released and final held. Both press and release in a single tick remain visible. The recorder rejects nonincreasing ticks, duplicate action/user keys, unregistered actions, invalid states and insufficient external buffer space before committing any bytes.

The tape takes an **externally owned byte buffer**, performs no memory allocation and has no native input/OS handles. The action set is caller-selected and is not inferred from per-platform key codes. Logical action IDs are stable across controller mapping/layout changes.

Playback uses `beginRead`, `next`, `query`, `rewind` and `seek`. The reader verifies frame structure, version, checksum, monotonic tick ordering, valid action IDs/users/values and unique action-user keys before advancing its cursor or overwriting the caller's frame.

## Little-endian version 1 frame format

A tape consists of sequential independent frames with no global header:

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 4 | `SIRP` magic |
| 4 | 2 | version, currently 1 |
| 6 | 2 | header size, 32 |
| 8 | 8 | simulation tick |
| 16 | 4 | action record count |
| 20 | 4 | complete frame byte length |
| 24 | 4 | FNV-1a32 checksum of the action payload |
| 28 | 4 | reserved zero |

Each action record is 20 bytes: logical action ID (4), user (4), signed value bit pattern (4), transition count (4) and flags (4: held/pressed/released in bits 0/1/2). The maximum frame size is 2,592 bytes for 128 actions. FNV-1a is an accidental-corruption check, **not** a security authentication primitive.

## Boundaries

Replay records **logical action results**, not raw per-device events, hardware identifiers, physical sensor samples, cursor movement, text, IME compositions or user-entered strings. This keeps normal gameplay replay compact and prevents capture of typed text by default.

This initial slice supports deterministic gameplay input inspection/consumption and tick seek but does not override a live `SIRInputService` or restore engine/gameplay world state. Full rollback will additionally need replay-mode ownership of the gameplay input source, deterministic world checkpoints, action-schema revisions and network rollback orchestration.

## Verification

`core.platform.input.replay` covers same-tick press/release counts, multi-user isolation, distinct tick records, record duplication, buffer limits, seek/rewind, corrupt checksum and truncated tape rejection. Source-level GCC/Clang 17 compilation and sanitizer tests may be run in a standalone test harness; full Windows/MSVC and Linux CTest on the exact branch SHA remain separate acceptance gates.

# SIREngine input history — bounded rollback window

**Owner:** `Source/Core/Platform/Input/SIRInputHistory`, building upon the phase-6 logical-action replay format.

## Ownership and lifetime

`SIRInputHistory` stores fixed-size logical-action frames in a caller-provided array of `sirInputReplayFrame_t`. It does not allocate, own or free memory. The caller chooses the history depth and obtains memory from the engine's current allocator. The history and its backing array remain bound until reinitialization or destruction by their owner. All mutation and reads occur on the same input owner thread; concurrent readers require an external publication protocol.

`initialize(storage, capacity)` binds storage and clears the ring. `append(frame)` accepts a fully validated frame only if its simulation tick follows the previous tick **by exactly one**. Missing/out-of-order ticks are failures, not silently skipped. At capacity, the oldest tick is discarded, with constant-time lookup by tick within the retained contiguous window. No frame pointer is exposed to callers; `read(tick, out)` copies the requested frame.

`rewindFrom(tick)` discards the selected tick and every newer tick; callers can then resimulate corrected inputs by appending new ticks. `replaceFrom(frame)` combines rewind and insertion for a corrected retained tick, invalidating all subsequent history. Out-of-window corrections are rejected. Applications must restore the corresponding **gameplay world checkpoint** separately; discarding future input frames is not itself a world rollback.

## Stable action digest

`hashFrame(frame, out)` and `digest(tick, out)` calculate FNV-1a64 over little-endian scalar encodings of the tick, action count, logical action/user IDs, Q15 values, transition counts and flags. Padding and unused action slots are excluded. Equal recorded input frames yield the same digest across platforms with compatible integer semantics. The hash detects mismatches and is not cryptographic authentication.

## Service integration

After publication, `SIRInputService::captureHistoryTick(history, keys, count)` samples authoritative logical-action states. It reads via `queryAction`, so recording observes active playback when present and live mapping otherwise. The service rejects capture during collection, oversized lists, unknown action keys and noncontiguous ticks.

During replay, after `begin(tick)`, `SIRInputService::queueHistoryTick(history, tick)` retrieves the matching recorded frame and delegates to the existing validated `queueReplayTick` path. A missing tick, mismatch or duplicate queue operation cannot substitute a different gameplay action result. The caller owns tick scheduling, correction policy, and world-state restoration.

## Validation and outstanding gates

`core.platform.input.history`: bounded ring overflow/eviction, out-of-window reads, missing-tick rejection, rewind/replace correctness, frame validation, padding-independent hashes, 10,000-tick stress. `core.platform.input.replay` adds service capture and authoritative playback-from-history tests.

Local isolated GCC 14 and Clang C++17 ASan/UBSan tests cover the history class against the existing replay codec with a minimal interface stub. Full SIREngine Windows/Linux CMake/CTest, repository-pinned formatting, hardware, renderer synchronization and network rollback are separate acceptance requirements. The input-history class does not implement network transport, world snapshots, rollback orchestration or IME/pointer recording.

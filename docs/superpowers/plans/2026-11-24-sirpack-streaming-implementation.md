# SIRPack Streaming Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or superpowers:subagent-driven-development) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the production SIRPack streaming path specified by the implementation brief, with rewritten contracts and no legacy compatibility layer.

**Architecture:** Package metadata and physical shard lifetimes are owned by `SIRPack`; VFS owns visibility and leases. Streaming owns bounded transfer/decode orchestration and uses the existing coordinator for tickets, while GRDG remains the sole canonical payload writer. All asynchronous records use fixed-capacity slot/generation storage.

**Tech Stack:** C++17, CMake, Core Jobs, SIR VFS, SIR platform file/mapping, existing GRDG, pinned ZSTD/LZ4 where available, CTest.

**Spec:** `docs/superpowers/specs/2026-11-24-sirpack-streaming-design.md` and `/home/noah/Drive2/noah/SIRPack-Streaming-Implementation-Brief.md`

## Global Constraints

- No legacy compatibility for placeholder SIRPack or codec contracts.
- SIRPack owns immutable package metadata and shard lifetimes; VFS owns mount ordering and leases.
- GRDG is the only canonical resident payload authority.
- All queues, pools, scratch, and work records are explicitly bounded.
- Stale generation, cancellation, and teardown checks occur at every asynchronous boundary.
- Source is C++17 and uses existing engine allocators/executors.

## Review Focus

- Corrupt/truncated catalog or shard must reject the entire mount without visibility.
- Short/overflowing/cross-shard reads must not publish or consume a pool lease.
- Codec output length, full input consumption, dictionary/version, and digest must all be checked.
- A later chunk publication must preserve prior resident chunks and stale sources must fail final publication.
- Cancellation/unmount/shutdown must drain exact owners before file or buffer destruction.

---

### Task 1: Freeze package and codec contracts

**Files:**
- Modify: `Source/Resources/SIRPackFormat.h`, `SIRPackCatalog.h`, `SIRPackRuntimeTypes.h`
- Replace: `Source/Resources/SIRPack.h`, `Source/Resources/SIRPack.cpp`
- Create: `Source/Resources/SIRPackShard.h`, `Source/Resources/SIRPackShard.cpp`, `Source/Resources/SIRCompressionTypes.h`, `Source/Resources/SIRCompressionCodec.h`, `Source/Resources/SIRCompressionRegistry.h`
- Test: `Tests/Unit/SIRPackFoundationTests.cpp`, `Tests/Unit/SIRPackCodecTests.cpp`

- [ ] Write failing tests for new descriptors, NONE decoding, compressed admission, malformed codec metadata, and package source resolution.
- [ ] Run the focused tests and verify they fail for missing/replaced interfaces.
- [ ] Implement explicit little-endian wire helpers, catalog validation, incremental digest, and immutable package/shard declarations.
- [ ] Implement registry dispatch and strict raw codec behavior; wire optional ZSTD/LZ4 without silently treating unavailable codecs as raw.
- [ ] Run the focused foundation/codec tests and the existing catalog fixture tests.

### Task 2: Implement mount ownership and leases

**Files:**
- Create: `Source/Resources/SIRPackMountService.h`, `SIRPackMountService.cpp`
- Create: `Source/Resources/SIRPackSourceLease.h`, `SIRPackSourceLease.cpp`
- Test: `Tests/Unit/SIRPackRuntimeTests.cpp`

- [ ] Write failing tests for rooted open, multi-shard validation, priority winner, overlay replacement, lease survival after unmount, and failed-open unwind.
- [ ] Implement stable shard storage, catalog mapping/owned fallback, package identity checks, and positional reads.
- [ ] Implement VFS mount/acquire/release integration and retired-package collection.
- [ ] Run runtime tests with corrupt, missing, stale, and traversal-path fixtures.

### Task 3: Implement bounded I/O

**Files:**
- Create: `Source/Streaming/SIRIOTypes.h`, `SIRIOBufferPool.h/.cpp`, `SIRIORequestPlanner.h/.cpp`, `SIRAsyncIOService.h`, `SIRJobIOService.h/.cpp`
- Test: `Tests/Unit/SIRPackIOTests.cpp`

- [ ] Write failing tests for bounded buffer leases, deterministic ordering, exact duplicate sharing, overlap fan-out, batch rollback, stale handles, cancellation, short reads, and completion backpressure.
- [ ] Implement generation-tagged O(1)-bounded pools and planner candidate/commit/discard semantics.
- [ ] Implement the Core Jobs backend using `SIRPack::readAt`, terminal observation, and owner-thread collection.
- [ ] Run focused I/O tests including zero-worker pumping and repeated shutdown.

### Task 4: Implement logical unit and prediction policy

**Files:**
- Create: `Source/Streaming/SIRStreamingUnitResolver.h/.cpp`, `SIRPredictionSystem.h/.cpp`
- Test: `Tests/Unit/SIRPackPredictionTests.cpp`

- [ ] Write failing tests for manifest generation, dependency-first expansion, cycle/missing bootstrap rejection, deterministic ordering, epoch replacement, and budget caps.
- [ ] Implement bounded immutable manifest replacement and non-recursive closure planning.
- [ ] Implement deterministic confidence/priority/deadline prediction as a PREPARED consumer through the existing reconciler.
- [ ] Run focused prediction/resolver tests.

### Task 5: Implement materialization and runtime composition

**Files:**
- Create: `Source/Streaming/SIRResourceStreamingCoordinator.h/.cpp`, `SIRStreamingRuntime.h/.cpp`
- Modify: `Source/Resources/SIRResourceGraph.h/.cpp` only for the reviewed provenance/reuse seam
- Modify: `Source/Resources/CMakeLists.txt`, `Source/Streaming/CMakeLists.txt`, `Tests/CMakeLists.txt`, relevant registration files
- Test: `Tests/Unit/SIRPackMaterializationTests.cpp`, `SIRPackIntegrationTests.cpp`, lifecycle tests

- [ ] Write failing tests for A→B preservation, digest failure, stale source, revision mismatch, shared backing, multiple waiters, delayed provider completion, and shutdown drain.
- [ ] Implement operation records and dependency-first read/decode/verify staging.
- [ ] Publish full replacement chunk sets through the GRDG writer only; preserve old records on failure.
- [ ] Implement bridge adaptation and runtime initialization/reverse-order shutdown.
- [ ] Register the integrated fixture and run it through real Core Jobs, codec path, GRDG, and delayed provider acknowledgement.

### Task 6: Full verification

- [ ] Configure a fresh build directory.
- [ ] Build affected targets and run all new CTest registrations plus the existing resource/streaming regression suite.
- [ ] Run formatting/standards checks on changed files and inspect the final diff for unrelated edits.
- [ ] Re-read the brief acceptance matrix and report any unavailable external dependency or provider coverage explicitly.

## Execution Handoff

This plan is being executed natively in the current authorized checkout because the user asked to continue implementation and did not request a worktree. Each task follows failing-test → minimal implementation → focused verification, followed by full-suite verification before any completion claim.

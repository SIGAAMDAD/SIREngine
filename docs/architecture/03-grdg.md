# 03 — Global Resource Dependency Graph (GRDG)

**Status:** proposed API and ownership contract; not yet implemented.

GRDG is the single authoritative registry for reusable asset/resource payloads consumed by rendering, physics, audio, navigation, gameplay and the editor. The target model is **one writer, multiple lock-free readers** for published immutable resource metadata and versioned payload views. Resource identity, dependency edges and residency ownership belong to GRDG, not to each consuming subsystem.

## Data model

```text
ResourceKey (stable type + cooked identifier)
  -> Slot (generation + state + published record pointer)
  -> Payload (CPU-owned canonical bytes / typed immutable representation)
  -> Dependency handles (canonical graph edges)
  -> Provider resident views (GPU/bank/collider handles; derived storage)
```

A resource handle must include an index and generation and carry an explicit type. Published versions are immutable. Single-writer mutation takes place off the read path; publication uses release semantics and reader loads use acquire semantics. Retire payloads only after all readers leave their protection epoch and all GPU/audio/physics consumers finish their fences or callbacks. The selected reclamation method (epochs/hazard-equivalent) must be backed by executable concurrency tests.

## System contracts

- A unique CPU-owned payload is stored once per resident identity, with consumers borrowing read-only scoped views.
- GPU buffers, physics acceleration structures and audio decoded buffers may require **distinct derived representations**, whose existence and byte cost are registered against the same resource ID.
- No raw pointer may be cached across GRDG publication or eviction boundaries.
- Dependencies are typed and validated; cycles have explicit handling at cook time or load time.
- Resource states include unloaded, requested, staging, resident, evicting and failed. Residency transitions expose owner, reason and byte deltas.
- Zero-copy is used when representation, alignment, lifetime and device address-space semantics actually permit it.

## Jobs and publication

Streaming resolves dependency closure, allocates the canonical payload, schedules decoding, and publishes a new immutable record only after validation. Readers retain their epoch/view during work. The sole writer retires old records and schedules reclamation after quiescence; garbage collection is bounded per frame, not hidden in reader hot paths.

## Acceptance

Property tests for handle reuse/generation, contention tests with a writer and many readers, ThreadSanitizer stress, forced failed loads, deterministic dependency-resolution tests, and traces of peak bytes/duplicates are required. Evaluate read-side overhead, publication stalls and cache misses under full gameplay load.

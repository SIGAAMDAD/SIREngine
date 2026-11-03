# 02 — Memory ownership, domains and budgets

**Status:** page-level `SIRMemoryManager` and `SIRMemorySegment` are ported under `Source/Core/Memory/`. Expanded allocation routing, attributed live-byte accounting and complete engine integration remain target contracts.

The goal is centralized accounting and reclamation of **all CPU memory allocated by Godot-derived runtime code and integrated engine modules**, with explicit domains and caller attribution. EASTL containers alone do not accomplish this; their allocators must be rebound to the SIR allocation interface during the container migration. Godot `memalloc`, paged pools, object allocators and third-party allocations need domain-specific adapters and tracing.

## Domains

| Domain | Lifetime and allocator intent |
| --- | --- |
| Bootstrap/core | Long-lived engine metadata, bounded virtual reservation and commit |
| Frame/transient | Linear/frame arenas, reset at known job/frame fences |
| Resource resident | Stable payload allocations; budgeted by GRDG residency |
| Streaming/staging | Short-lived I/O/decompression/cooking staging |
| Physics | Long-lived narrowphase data and per-step scratch, explicit Jolt integration |
| Rendering CPU | Draw/descriptor scheduling and CPU command scratch; GPU tracked separately |
| Audio | FMOD callback-backed domain initially, later other provider callback paths |
| Editor-only | Independent, generous budget that cannot mask shipping-runtime budgets |
| Gameplay/networking | Game-supplied capacity and packet arena policies |

Godot's efficient paged allocators may remain as *suballocators* if their backing allocations, retained pages and release behavior are fully traced to SIRMemoryManager and its future allocation router. Replacing a fast pool solely to achieve uniformity is not the objective; central ownership, accounting and bounded reclamation are. No untracked external-heap fallbacks are accepted for a shipping budget profile.

## Allocation contract

Every allocation records at minimum domain, requested size, committed size, alignment and a source-backed owner tag. Domains expose high-water, current committed/resident, allocation rate, failure count and retained capacity. Free/reuse must respect reader epochs, GPU fences, audio callbacks and asynchronous jobs.

The engine must support multiple **game-specific** budget configurations. The Nomad's intended 512 MiB envelope is a game profile, not a hard-coded limit on Sigilbound or the editor. CPU committed memory, GPU device allocations, memory-mapped files and external library reservations are reported separately.

## Acceptance

An allocation trace must cover startup, editor import, scene load, streaming traversal, destruction, save/load and shutdown. Compare allocation paths against an exhaustive object/source inventory, then run long-lived soak tests and stress/failure injection. A clean trace needs no unexplained untracked CPU allocations and no unreclaimed pages outside declared caches. Report real peaks, not estimates based on the sum of subsystem names.

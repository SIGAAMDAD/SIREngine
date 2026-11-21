---
name: SIR memory and container ownership
description: Enforce allocator routing, lifetime, compact storage and container behavior.
applyTo: 'Source/Core/Memory/**,Source/Core/Containers/**,Source/Core/Strings/**,Source/Core/Objects/**,Tests/**/*Memory*,Tests/**/*Container*'
---
# Memory and ownership

- Identify the owner, lifetime, aliasing, move/copy behavior and allocation domain before changing storage.
- Route engine allocations through SIREngine allocator infrastructure. The most recent contract permits globally overridden `new` and `delete` when routed through the engine allocator; do not replace these with untracked runtime heap allocations.
- Do not introduce `malloc`/`calloc`/`realloc`/`free` or ad hoc standard smart-pointer ownership in first-party runtime systems. At required foreign ABI boundaries, document the allocator and who frees the object.
- Use existing engine containers, EASTL where integrated, and compact POD storage with explicit ownership. Do not add `std::vector`, `std::string_view`, generic `using` aliases, or standard algorithm wrappers to new first-party hot paths.
- Replace fundamental `std::` copying, filling and movement helpers with approved engine/EASTL counterparts or explicit safe loops when appropriate. Preserve object construction/destruction correctness; `memcpy` is only for trivially copyable bytes.
- Verify alignment, overflow, null/empty states, iterator invalidation, in-place growth, exception-free failures and allocator accounting for containers.
- Do not accept performance claims without workload, baseline, compiler, build config, distribution, and repeatability evidence.

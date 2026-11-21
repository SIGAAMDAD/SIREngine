---
name: SIR threading and jobs
description: Apply engine synchronization and execution ownership rules.
applyTo: 'Source/Core/Jobs/**,Source/Core/Systems/**,Source/Core/Events/**,Source/Renderer/**,Source/Streaming/**,Tests/**/*.{cpp,h}'
---
# Concurrency

- Engine-owned synchronization uses existing SIR primitives; no newly introduced `std::thread`, `std::mutex`, `std::condition_variable`, `std::future`, `std::async` or `std::atomic` in first-party systems.
- Write down task ownership, thread affinity, producer/consumer publication, memory visibility, cancellation and teardown before modifying concurrent code.
- Keep render-thread ownership of graphics API submission and device objects. Do not move render-driver operations to worker threads without an explicitly reviewed contract.
- Prefer independent worker-owned scratch state over shared mutable state. Maintain single-writer world/physics state where established.
- Add deterministic race/lifetime tests, shutdown tests, and sanitizer runs when applicable. ThreadSanitizer and real hardware are environment-dependent; report NOT RUN if unavailable.
- Preserve platform-specific synchronization behind the engine API; avoid platform locks in shared gameplay/rendering code.

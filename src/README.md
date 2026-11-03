# SIREngine-owned source

Source modules are introduced incrementally after their architectural contracts and exact-source build dependencies are established. The authoritative Godot fork remains pinned under `engine/godot` during bootstrap.

- `core/`: allocator domains, handles, jobs and low-level utilities
- `resources/`: GRDG, residency, dependency graph, streaming
- `rendering/`: graphics provider adapters and GPU resource views
- `audio/`: FMOD first, then OpenAL and Wwise providers
- `physics/`: physics provider and Jolt integration
- `editor/`: Godot editor adapters, import pipelines, diagnostics
- `cooker/`: deterministic cooked asset formats and builders
- `platform/`: OS-specific integration
- `networking/`: generic transport/packet plumbing, not game packet schemas

Directories without implementation are intentionally documented, not populated with empty compile targets.

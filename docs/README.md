# SIREngine technical documentation

The numbered architecture contracts below define **intended** subsystem interfaces and acceptance criteria. A proposed feature becomes implemented only when corresponding code, tests, measured performance and target-platform evidence exist. [Decision records](adr/0001-source-strategy.md) explain architectural choices; [the foundation roadmap](roadmap/foundation.md) tracks implementation order.

| Document | Contract |
| --- | --- |
| [00 – Vision](architecture/00-vision.md) | Engine identity, reusable-game scope and constraints |
| [01 – Boundaries](architecture/01-system-boundaries.md) | Runtime layers and module dependencies |
| [02 – Memory](architecture/02-memory-and-allocators.md) | Allocation accounting, pool domains and reclamation |
| [03 – GRDG](architecture/03-grdg.md) | Single-writer/multiple-reader shared resource ownership |
| [04 – Containers](architecture/04-containers.md) | EASTL types, aliases and allocator wiring |
| [05 – Rendering](architecture/05-rendering-backends.md) | Vulkan, D3D12, OpenGL and driver negotiation |
| [06 – Audio](architecture/06-audio-backends.md) | FMOD, Wwise and OpenAL provider boundaries |
| [07 – Editor/cooker/runtime](architecture/07-editor-cooking-runtime.md) | Authoring representation to compact streamed data |
| [08 – Threading](architecture/08-threading-and-jobs.md) | Work ownership, queues, deterministic shutdown |
| [09 – Platforms](architecture/09-platform-support.md) | Windows/Linux first, macOS deferred |
| [10 – Build/deps](architecture/10-build-and-dependencies.md) | Toolchains, CMake/SCons and pinned upstream dependencies |
| [11 – Validation](architecture/11-acceptance-and-validation.md) | Compiler, AST, format, runtime and CI gates |
| [12 – Games](architecture/12-game-integration.md) | Interfaces for The Nomad, Sigilbound and other games |
| [13 – Source migration](architecture/13-migration-plan.md) | Practical migration from pinned Godot source |

SIREngine-owned C++ follows [the formatting and generation standard](standards/cpp.md). For design changes, add an ADR rather than silently editing a contract.

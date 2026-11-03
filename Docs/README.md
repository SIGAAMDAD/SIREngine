# SIREngine documentation

The technical documentation separates implemented source, architectural contracts, and staged migration work. The native C++17 foundation is buildable; the inherited Godot, Terrain3D, and NomadFramework repositories are currently pinned references under `Upstream/`.

## Milestones

The project's four implementation milestones are recorded in [the milestone index](Milestones/README.md):

1. [Port NomadFramework C++ systems](Milestones/01-NomadFramework.md)
2. [Port Godot startup and editor](Milestones/02-Godot-Startup-Editor.md)
3. [Port Terrain3D](Milestones/03-Terrain3D.md)
4. [Begin GRDG development](Milestones/04-GRDG.md)

## Architecture contracts

| Contract | Document |
| --- | --- |
| Engine identity and design scope | [00 - Vision](Architecture/00-vision.md) |
| System boundaries and dependency direction | [01 - Boundaries](Architecture/01-system-boundaries.md) |
| Memory ownership and allocation domains | [02 - Memory](Architecture/02-memory-and-allocators.md) |
| Resource ownership and publication | [03 - GRDG](Architecture/03-grdg.md) |
| C++17 container and allocator policy | [04 - Containers](Architecture/04-containers.md) |
| Rendering drivers and resource views | [05 - Rendering](Architecture/05-rendering-backends.md) |
| Audio provider boundaries | [06 - Audio](Architecture/06-audio-backends.md) |
| Editor, cooking and shipping data | [07 - Runtime data](Architecture/07-editor-cooking-runtime.md) |
| Job scheduling and concurrency | [08 - Threading](Architecture/08-threading-and-jobs.md) |
| Windows, Linux and macOS | [09 - Platforms](Architecture/09-platform-support.md) |
| Build systems and dependencies | [10 - Build](Architecture/10-build-and-dependencies.md) |
| Testing and acceptance evidence | [11 - Validation](Architecture/11-acceptance-and-validation.md) |
| Reusable interfaces and game modules | [12 - Integration](Architecture/12-game-integration.md) |
| Engine source migration | [13 - Migration](Architecture/13-migration-plan.md) |

See [C++ standards](Standards/cpp.md) and [architecture decisions](Decisions/0001-source-strategy.md) for established conventions and provenance.

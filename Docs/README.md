# SIREngine documentation

Architecture documents distinguish the native C++17 foundation from systems still under development. `Source/` is the authority for first-party engine code, organized directly by subsystem. External engine references are consulted during planned source ports without being tracked as nested source repositories.

## Implementation milestones

1. [NomadFramework C++ services](Milestones/01-NomadFramework.md)
2. [Godot-derived startup and editor](Milestones/02-Godot-Startup-Editor.md)
3. [SIREngine Terrain](Milestones/03-Terrain.md)
4. [Global Resource Dependency Graph](Milestones/04-GRDG.md)

## Architectural specifications

| System | Specification |
| --- | --- |
| Identity and goals | [00 - Vision](Architecture/00-vision.md) |
| Engine boundaries | [01 - Boundaries](Architecture/01-system-boundaries.md) |
| Memory ownership | [02 - Memory](Architecture/02-memory-and-allocators.md) |
| Shared resources | [03 - GRDG](Architecture/03-grdg.md) |
| Containers and allocators | [04 - Containers](Architecture/04-containers.md) |
| Rendering | [05 - Rendering](Architecture/05-rendering-backends.md) |
| Audio | [06 - Audio](Architecture/06-audio-backends.md) |
| Editor and cooked runtime | [07 - Editor/runtime](Architecture/07-editor-cooking-runtime.md) |
| Threading and jobs | [08 - Threading](Architecture/08-threading-and-jobs.md) |
| Platform targets | [09 - Platforms](Architecture/09-platform-support.md) |
| Build and dependencies | [10 - Build](Architecture/10-build-and-dependencies.md) |
| Verification | [11 - Validation](Architecture/11-acceptance-and-validation.md) |
| Game integration | [12 - Integration](Architecture/12-game-integration.md) |
| Migration sequence | [13 - Migration](Architecture/13-migration-plan.md) |

See [dependency setup](Dependencies.md), [managed scripting architecture](Architecture/14-managed-scripting.md), and [engineering standards](Standards/cpp.md), [architectural decisions](Decisions/0001-source-strategy.md), and [the milestone index](Milestones/README.md).

The [source-tree decision](Decisions/0003-flat-source-tree.md) documents the adopted uppercase, subsystem-oriented hierarchy and the preserved CMake/FASTBuild boundaries.

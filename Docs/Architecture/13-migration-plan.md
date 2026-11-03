# 13 - Engine migration plan

**Status:** C++17 native bootstrap implemented; four source-port milestones scheduled.

The new repository separates first-party `Engine/Source/`, optional `Modules/`, external libraries `ThirdParty/`, and pinned source-origin references `Upstream/`. Original upstream trees are not linked into the native launcher until specific interfaces and source dependencies are ported.

1. **[Port NomadFramework C++ systems](../Milestones/01-NomadFramework.md)** - Establish reusable native Core services and appropriate allocator ownership from the authenticated sources. Carry source provenance, C++17 integration, tests and Windows/Linux compilation.
2. **[Port Godot startup and editor](../Milestones/02-Godot-Startup-Editor.md)** - Introduce the real Godot-derived application lifecycle, platform entrypoints, object/scene services, editor functionality and CMake build registrations into SIREngine-owned code.
3. **[Port Terrain3D](../Milestones/03-Terrain3D.md)** - Integrate the retained terrain editing and runtime systems, source shader dependencies and serialized-region behavior.
4. **[Begin GRDG development](../Milestones/04-GRDG.md)** - Implement and test canonical resource identities, one-writer/multiple-reader published records and explicit reclamation as the first independent resource-graph slice.

Do not complete a milestone by importing source files that do not build or by replacing verified upstream behavior with an untested approximation. Memory and allocator integration remains a shared contract across the ports; the full graph-wide consumer migration extends beyond the fourth milestone.

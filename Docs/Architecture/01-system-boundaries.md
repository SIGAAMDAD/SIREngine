# 01 - Engine dependency and ownership boundaries

**Status:** target architecture; portable Core and Launcher bootstrap available.

```text
Application/game
      |
Editor / Programs / Developer tools
      |
   Runtime
  /  |   \_________________________________
Terrain World Rendering Resources Physics Audio ...
      |
     Core
      |
Platform / operating system
```

- `Engine/Source/Core/`: portable memory, jobs, containers, mathematics and common engine services.
- `Engine/Source/Runtime/`: built-in systems for terrain, resources, graphics, physics, world state, animation, audio, networking and scripting.
- `Engine/Source/Editor/`: editing and import tools, including built-in terrain authoring.
- `Engine/Source/Developer/` and `Programs/`: diagnostics, processing utilities and executable entrypoints.
- `Engine/Source/Platform/`: OS-specific services.
- `ThirdParty/`: version-pinned third-party libraries used by explicit engine integrations.

Terrain is engine-owned at `Engine/Source/Runtime/Terrain/`, with editor tooling at `Engine/Source/Editor/Terrain/`. Its CPU state and region formats belong to Terrain; rendering, physics, navigation and the resource graph consume well-defined interfaces rather than introducing competing terrain authorities.

Portable Core cannot depend on rendering, Terrain, the editor or game-specific systems. Runtime systems use Core and platform interfaces, while the editor consumes runtime APIs. Each subsystem declares mutable state ownership, allocation domains, reader lifetimes, serialization boundaries and shutdown ordering.

Porting external implementations is a source-level migration into the appropriate engine directories, with native compilation, license preservation and appropriate tests. The source directory alone is not implementation evidence.

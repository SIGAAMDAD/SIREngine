# 01 - Subsystem boundaries and resource ownership

**Status:** target architecture; the Core/Launcher bootstrap is implemented.

SIREngine uses direct subsystem ownership in `Source/`. The directory tree reflects runtime responsibilities, not separate editor/runtime/program packaging layers.

```text
Source/Main.cpp      Tools/          Source/Editor/
       \               |                 /
               Source/Engine/
                     |
     Source/Scene/    Source/World/
          |                |
     Resources -- Terrain -- Renderer -- Physics
          |        |            |
        Mesh    Navigation     Audio / Scripting / Networking
                     |
              Core / System
                     |
              OS and libraries
```

Subsystem APIs point toward their stable dependencies rather than forming a cycle through editor or application entrypoints. The Portable Core and System layers have no dependency on renderer, Terrain, UI, scene, or editor. The editor consumes the same runtime APIs as the application where practical; specialized terrain authoring lives under `Source/Terrain/Editor/`.

## Source ownership

| Path | Ownership |
| --- | --- |
| `Source/Core/` | Portable types, memory, containers, math, threading and bootstrap services |
| `Source/System/` | OS, process, virtual memory and filesystem integration |
| `Source/Engine/` | Lifecycle, registration and application coordination |
| `Source/Scene/` and `Source/World/` | Scene state, spatial organization and streaming |
| `Source/Resources/` | GRDG, canonical identity, residency and dependency graphs |
| `Source/Renderer/` | Graphics-device state, materials and driver backends |
| `Source/Terrain/` | Built-in terrain storage, runtime operations and specific authoring |
| `Source/Mesh/`, `Source/Navigation/`, `Source/Physics/` | Geometry, navigation and simulation |
| `Source/Animation/`, `Source/Audio/`, `Source/Scripting/`, `Source/Networking/`, `Source/UI/` | Other first-party runtime systems |
| `Source/Editor/` | General editor, inspector, scene and import infrastructure |
| `Tools/` | Offline programs and diagnostic tools |

A resource has one canonical identity and one declared mutable owner. GPU, collision, navigation and decoded-audio representations can have different storage and lifetimes but must be accounted for under the resource's identity. Terrain-specific state is owned by Terrain; it does not take ownership of the graphics device or duplicate GRDG resource storage.

The current executable compiles `Source/Main.cpp` and the static `SIRCore` target in `Source/Core/`. Other directories are integration boundaries, not claims that the full engine or editor is implemented. Future source ports need actual build dependencies, allocation contracts and runtime tests.

# 01 - SIREngine system boundaries

**Status:** target dependency model, with a minimal native Core implemented.

```text
Applications and game modules
          |
Editor / Programs / Tools / Developer
          |
        Runtime
    /   /   |   \
 Resource  World  Rendering  Audio  Physics ...
          |
         Core
          |
     Platform / OS
```

`Engine/Source/Core/` owns the portable foundation: initialization, interfaces, memory services, containers, threading, mathematics, logging, filesystem and basic types as individual systems are ported. `Engine/Source/Runtime/` houses render and audio backends, physics, resources, world/streaming, networking, animation and scripting. `Engine/Source/Editor/`, `Developer/`, `Programs/` and `Platform/` are distinct build boundaries. `Modules/` is reserved for optional plugins such as Terrain3D.

The portable Core has no dependency on renderer, editor, gameplay or third-party scene-object APIs. Runtime uses Core and applicable platform services. Editor consumes runtime interfaces; backend-specific details are kept behind driver interfaces. Static objects can be cooked into compact runtime representations without sacrificing editor round-tripping.

`Upstream/Godot/`, `Upstream/NomadFramework/` and `Upstream/Terrain3D/` are pinned source references, not built dependencies of the native launcher. They become first-party SIREngine code only through independently validated migration slices. Source trees are not accepted as implemented by creating folders or CMake targets.

All stateful systems define a single mutating owner, scoped reader lifetimes, allocation domain, serialization/versioning contract where applicable, shutdown order and failure behavior. Shared resource identity eventually belongs to GRDG.

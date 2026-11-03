# SIREngine source

First-party C++17 systems are organized directly under `Source/` by subsystem. Source headers and implementations live together; no separate `Runtime/`, `Public/`, `Private/` or program hierarchy is imposed.

| Directory | Responsibility |
| --- | --- |
| `Core/` | Low-level shared types, memory, math, containers, threading and temporary bootstrap lifecycle |
| `System/` | Platform, OS services, filesystem and process integration |
| `Engine/` | Engine initialization, registration and main-loop ownership as these are ported |
| `Scene/` | Scene objects, serialization and scene graph |
| `Resources/` | Global resource identities, GRDG, streaming and asset catalogs |
| `Renderer/` | Graphics-device interfaces, Vulkan, D3D12, OpenGL and material rendering |
| `Terrain/` | Built-in terrain runtime and terrain-specific editor integration |
| `World/` | Spatial state and world streaming |
| `Mesh/` | Geometry and meshoptimizer integration |
| `Physics/` | Collision and physics backend |
| `Navigation/` | Navigation data and path queries |
| `Animation/` | Animation systems |
| `Audio/` | FMOD and OpenAL Soft provider interfaces |
| `Scripting/` | C# / Mono embedding and other scripting |
| `Networking/` | Generic networking and transport |
| `UI/` | Runtime user interface |
| `Editor/` | Shared editor, inspector, import and authoring infrastructure |
| `Main.cpp` | Native executable entrypoint |

Directories are added when ownership needs to be established; empty subsystems are not linked into the build. During bootstrap, the buildable code consists of `Core/SIRApplication.h`, `Core/SIRApplication.cpp` and `Main.cpp`. `SIRCore` remains a static library until further engine systems are migrated.

Compile and test using the root CMake presets. The source tree uses PascalCase naming for first-party directories and filenames.

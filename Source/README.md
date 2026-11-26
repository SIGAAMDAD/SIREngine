/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

# SIREngine source

First-party C++17 systems are organized by direct subsystem ownership. Source headers and implementations live together; no artificial `Runtime/`, `Public/`, `Private/` or program hierarchy is imposed. The table describes **target ownership**, not a claim that every subsystem has implementation files.

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

Subsystem directories are created when they receive source or concrete editor integration work. Empty README-only subsystem placeholders have been removed. The root `CMakeLists.txt` composes major systems through their individual `Source/<System>/CMakeLists.txt` files. There is no intermediate `Source/CMakeLists.txt` or target-authoring CMake file per source folder. Several backend and test targets are optional. These targets contain implementation at different maturity levels; their presence does not establish production acceptance. `SIRCore` is a static library linked into the native `SIREngine` executable, whose target is declared at the project root.

Compile and test using the root CMake presets. The source tree uses PascalCase naming for first-party directories and filenames.

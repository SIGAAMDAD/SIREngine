# SIREngine

[![Native CI (develop)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/ci.yml?query=branch%3Adevelop)
[![Native CI (main)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/ci.yml?query=branch%3Amain)
[![Nightly (main)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/nightly.yml/badge.svg?branch=main)](https://github.com/SIGAAMDAD/SIREngine/actions/workflows/nightly.yml?query=branch%3Amain)
[![Codecov (develop)](https://codecov.io/gh/SIGAAMDAD/SIREngine/branch/develop/graph/badge.svg)](https://app.codecov.io/gh/SIGAAMDAD/SIREngine/tree/develop)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)](https://en.cppreference.com/w/cpp/17.html)
[![License: MPL 2.0](https://img.shields.io/badge/License-MPL%202.0-blue.svg)](LICENSE)

SIREngine is an open-source, general-purpose engine for building 2D and 3D games and interactive applications. Originating from Godot, the project is developing an independent native codebase while retaining a focus on accessible editing, flexible workflows, and cross-platform development.

The engine is designed to support projects of different scales and technical requirements. Its engineering priorities are predictable performance, efficient resource management, well-defined subsystem boundaries, and practical tools for creating, testing, and shipping content. No specific game, genre, or resource budget defines the engine's public architecture.

## Direction

SIREngine's development is organized around a small set of enduring goals:

- **An integrated authoring environment:** scene editing, asset importing, scripting, inspection, and debugging available from a coherent set of tools.
- **A configurable native runtime:** rendering, physics, audio, networking, and other systems with explicit dependencies and platform implementations.
- **Efficient resource use:** traceable memory ownership, reusable asset data, streaming, and instrumentation that makes runtime costs visible.
- **An open, extensible architecture:** clear interfaces for contributors and games to build specialized capabilities without changing unrelated engine systems.
- **Reliable engineering:** reproducible builds, compiler and runtime tests, versioned dependencies, and automated development checks.

These are design goals, not a claim that all of the corresponding systems have been implemented.

## Development status

The `develop` branch is a C++17 and CMake foundation with a minimal native application and unit tests. The complete editor and runtime are being developed as SIREngine-owned systems. Terrain is a built-in runtime subsystem alongside rendering, physics, audio and resource management; the terrain runtime and editor tools are still under development.

Development is grouped into four [milestones](Docs/Milestones/README.md): porting reusable native framework systems, bringing over engine startup and editor functionality, integrating terrain tooling, and starting the shared resource graph. Further architectural details and implementation status are maintained in [Docs](Docs/README.md).

## Building

Requirements: CMake 4.4.5, FASTBuild 1.20, a C++17 compiler, and Git. Ninja is optional for compiler tooling. The native foundation build has no mandatory external SDK or scripting runtime.

```sh
git clone --branch develop https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The same `dev` preset works on Windows and Linux. `debug`, `profile` and `release` provide other build types; `cmake --build --preset release --target package` packages the executable. macOS is an experimental build target. Optional dependencies are fetched separately for the corresponding development milestones.

## Source organization

| Directory | Purpose |
| --- | --- |
| `Source/Core/` | Portable foundation and common engine services |
| `Source/Renderer/`, `Source/Terrain/`, `Source/Resources/` | Built-in graphics, terrain and resource ownership |
| `Source/World/`, `Source/Scene/`, `Source/Physics/` | World, scene and simulation systems |
| `Source/Audio/`, `Source/Scripting/`, `Source/Networking/` | Media, C# scripting and network integration |
| `Source/Editor/`, `Source/Terrain/Editor/` | Shared editor and terrain-specific authoring |
| `Source/System/` | Operating-system services and platform integration |
| `Source/Main.cpp` | Native application entrypoint |
| `Build/` | CMake, toolchain and automation infrastructure |
| `ThirdParty/` | Pinned dependencies |
| `Tests/` | Native unit and integration tests |
| `Docs/` | Architecture, standards, decisions and milestones |

## Contributing and licensing

See [Contributing](CONTRIBUTING.md), [coding standards](Docs/Standards/cpp.md), and [governance](GOVERNANCE.md). SIREngine-owned source is licensed under the [Mozilla Public License 2.0](LICENSE); third-party components retain their own terms, as documented in [third-party notices](THIRD_PARTY_NOTICES.md).

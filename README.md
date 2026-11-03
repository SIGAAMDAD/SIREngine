# SIREngine

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

The `develop` branch is a fresh C++17 and CMake foundation. It builds a small native application and unit tests without compiling the upstream engines. Godot, NomadFramework, and Terrain3D are available as pinned source references for the scheduled ports. The editor and complete game runtime are not yet part of the native executable.

Development is grouped into four [milestones](Docs/Milestones/README.md): porting reusable native framework systems, bringing over engine startup and editor functionality, integrating terrain tooling, and starting the shared resource graph. Further architectural details and implementation status are maintained in [Docs](Docs/README.md).

## Building

Requirements: CMake 4.4.5, a C++17 compiler, Ninja, and Git. The native foundation build has no mandatory external SDK or scripting runtime.

```sh
git clone --branch develop https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci
```

Use `windows-ci` on Windows. `linux-release` and `windows-release` produce release-configured binaries, and `cmake --build --preset linux-release --target package` packages the executable. macOS is an experimental build target. Optional dependencies are fetched separately for the corresponding development milestones.

## Source organization

| Directory | Purpose |
| --- | --- |
| `Engine/Source/Core/` | Native foundation and common services |
| `Engine/Source/Runtime/` | Runtime subsystems and backend integrations |
| `Engine/Source/Editor/` | Editor and authoring functionality |
| `Engine/Source/Developer/` | Diagnostics and developer-facing support |
| `Engine/Source/Programs/` | Executables and command-line programs |
| `Engine/Source/Platform/` | Platform-specific implementations |
| `Modules/` | Optional engine modules |
| `Build/` | CMake, toolchain and automation infrastructure |
| `ThirdParty/` | Pinned dependencies |
| `Upstream/` | Pinned source references for migration |
| `Tests/` | Native unit and integration tests |
| `Docs/` | Architecture, standards, decisions and milestones |

## Contributing and licensing

See [Contributing](CONTRIBUTING.md), [coding standards](Docs/Standards/cpp.md), and [governance](GOVERNANCE.md). The repository retains its existing [license](LICENSE); third-party components retain their own terms, as documented in [third-party notices](THIRD_PARTY_NOTICES.md).

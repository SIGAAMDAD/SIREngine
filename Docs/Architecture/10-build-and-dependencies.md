# 10 - Build and dependency contract

**Status:** native C++17 Core/Launcher available; editor, Terrain and other runtime ports are separate milestones.

## Source and build organization

- Root `CMakeLists.txt` defines SIREngine's native C++17 build and CPack packaging.
- `Engine/Source/Core/` builds the portable `SIRCore` library.
- `Engine/Source/Programs/Launcher/` builds the bootstrap `SIREngine` application.
- `Engine/Source/Runtime/Terrain/` and `Engine/Source/Editor/Terrain/` are permanent first-party subsystem boundaries. Their native implementation and build registrations arrive with the Terrain milestone.
- `Tests/Unit/` contains lifecycle and application tests registered with CTest.
- `Build/CMake/` owns dependency resolution, formatting, hygiene and build-tool options.
- `ThirdParty/` contains only pinned external library dependencies (EASTL, EABase and EAAssert). Proprietary SDKs remain separately resolved.

CMake **4.4.5** is the supported configuration entrypoint, with C++17 compiler extensions disabled. FASTBuild **1.20** is the primary compilation backend for the supplied presets, with incremental dependency tracking and a compiler artifact cache. Ninja is available through the tooling preset for Clang compilation databases. The native build does not invoke SCons or Python.

## Build commands

```sh
git clone --branch develop https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release --target package
```

Windows and Linux use the same developer presets. Other configurations include `debug`, `profile`, `release-ipo`, `bulk`, `asan`, `ubsan` and Ninja `tooling`; platform availability varies by compiler. macOS remains a later platform gate.

## CI/CD

`.github/workflows/ci.yml` validates tracked paths, native C++ formatting and Windows/Linux Debug builds with CTest. A native setup action installs CMake and FASTBuild without Python. `.github/workflows/nightly.yml` runs Release builds, tests, package artifacts, profiling and relevant sanitizers. FASTBuild uses platform/configuration-scoped compilation caches, and Ninja tooling jobs export `compile_commands.json`.

The source tree stores no nested Godot or terrain engine checkout. Migration inputs are verified separately by their own source identity and incorporated into first-party build targets through focused ports.

CI outcomes are recorded from real workflow runs; a successful native bootstrap does not imply editor, graphics or terrain functionality.

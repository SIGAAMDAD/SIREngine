# 10 - Build and dependency contract

**Status:** native C++17 Core/Launcher available; editor, Terrain and other runtime ports are separate milestones.

## Source and build organization

- Root `CMakeLists.txt` defines SIREngine's native C++17 build and CPack packaging.
- `Source/Core/` builds the portable `SIRCore` static library, with declarations and definitions colocated.
- `Source/Main.cpp` is the bootstrap `SIREngine` executable entrypoint; the root `CMakeLists.txt` declares its native target.
- `Source/Terrain/` is the built-in terrain subsystem, including `Source/Terrain/Editor/` for terrain-specific authoring. Actual terrain implementation will join the native build with milestone 3.
- `Tests/CMakeLists.txt` owns every full-engine test/benchmark target and CTest registration; test sources remain under `Tests/Unit/`, `Tests/Benchmarks/` and `Tests/Dependencies/`.
- One `CMakeLists.txt` per major system owns the complete system target/source list. `Source/Core/CMakeLists.txt` and `Source/Renderer/CMakeLists.txt` also own their platform and graphics-driver source/target configuration, respectively. `Tools/CMakeLists.txt` owns all cooker executables and libraries. `Build/CMake/` retains reusable dependency, formatting and build-tool helpers.
- `ThirdParty/` contains pinned source dependencies (EASTL, EABase, EAAssert, OpenAL Soft and meshoptimizer) plus documentation for separately installed FMOD and Mono SDKs. Wwise remains optional.

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

Windows and Linux use the same developer presets; unsupported macOS preview presets are omitted until the native platform exists. Other configurations include `debug`, `profile`, `release-ipo`, `bulk`, `asan`, `ubsan` and Ninja `tooling`; platform availability varies by compiler. macOS remains a later platform gate.

## CI/CD

`.github/workflows/ci.yml` runs changed-file formatting and Linux GCC, Linux Clang and Windows MSVC CMake/CTest checks. `.github/workflows/qualification.yml` runs on-demand LLVM 23 source qualification, sanitizers, backend tests and release packaging. Its scheduled nightly jobs build and test Release plus sanitizer configurations and publish native benchmark results. Nightly jobs freeze one `develop` source SHA.

GoogleTest and Google Benchmark are pinned as independent optional development dependencies. Native CI enables GoogleTest for CTest on Windows/Linux; nightly profiling builds and runs Google Benchmark and retains per-platform JSON measurements and median comparisons. Neither vendor library is linked into the shipping Core or Launcher.

Dependency-enabled configurations resolve `SIR::MeshOptimizer`, `SIR::OpenAL`, `SIR::FMODCore`, `SIR::FMODStudio` and `SIR::MonoRuntime` using `Build/CMake/SIRDependencies.cmake`, `SIRAudio.cmake` and `SIRMono.cmake`. The first two use pinned source submodules; FMOD and Mono use an installed SDK (or pkg-config for Mono). The OpenAL Soft and meshoptimizer targets have optional compiler/link smoke tests.

See [Dependency acquisition](../Dependencies.md). The source tree stores no nested engine checkout. Engine migration inputs are reviewed separately and ported into first-party build targets.

### Nightly execution

Nightly builds use the same CMake presets and CTest commands as developer builds. The workflow reports actual native build/test failures and benchmark artifacts. No extra validation scripts, generated gate files or custom CTest accounting wrappers are required.

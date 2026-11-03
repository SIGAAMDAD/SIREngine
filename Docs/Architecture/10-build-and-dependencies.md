# 10 - Build and dependency contract

**Status:** native C++17 Core/Launcher available; editor, Terrain and other runtime ports are separate milestones.

## Source and build organization

- Root `CMakeLists.txt` defines SIREngine's native C++17 build and CPack packaging.
- `Source/Core/` builds the portable `SIRCore` static library, with declarations and definitions colocated.
- `Source/Main.cpp` is the bootstrap `SIREngine` executable entrypoint; `Source/CMakeLists.txt` declares its native target.
- `Source/Terrain/` is the built-in terrain subsystem, including `Source/Terrain/Editor/` for terrain-specific authoring. Actual terrain implementation will join the native build with milestone 3.
- `Tests/Unit/` contains lifecycle and application tests registered with CTest.
- `Build/CMake/` owns dependency resolution, formatting, hygiene and build-tool options.
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

Windows and Linux use the same developer presets. Other configurations include `debug`, `profile`, `release-ipo`, `bulk`, `asan`, `ubsan` and Ninja `tooling`; platform availability varies by compiler. macOS remains a later platform gate.

## CI/CD

`.github/workflows/ci.yml` validates tracked paths, native C++ formatting and Windows/Linux Debug builds with CTest. A native setup action installs CMake and FASTBuild without Python. `.github/workflows/nightly.yml` runs Release builds, tests, package artifacts, profiling and relevant sanitizers. FASTBuild uses platform/configuration-scoped compilation caches, and Ninja tooling jobs export `compile_commands.json`.

Dependency-enabled configurations resolve `SIR::MeshOptimizer`, `SIR::OpenAL`, `SIR::FMODCore`, `SIR::FMODStudio` and `SIR::MonoRuntime` using `Build/CMake/SIRDependencies.cmake`, `SIRAudio.cmake` and `SIRMono.cmake`. The first two use pinned source submodules; FMOD and Mono use an installed SDK (or pkg-config for Mono). The OpenAL Soft and meshoptimizer targets have optional compiler/link smoke tests.

See [Dependency acquisition](../Dependencies.md). The source tree stores no nested engine checkout. Engine migration inputs are reviewed separately and ported into first-party build targets.

### Nightly regression history

The `.github/workflows/nightly.yml` schedule runs at **07:17 UTC** on `develop`. It compiles and tests Windows/Linux `profile` and `release` targets, runs Linux ASan/UBSan configurations, and checks the OpenAL Soft/meshoptimizer integration and Clang tooling. macOS is available by manual dispatch only.

The Bash-driven `Build/Scripts/NightlyBuild.sh` uses CMake/FASTBuild (or Ninja for tooling). Each configuration records configure/build/test/package exit codes and wall time, CTest JUnit results, compiler warning counts, and executable size in a JSON artifact. Compiler/test logs and Release ZIPs are retained separately; artifacts contain the exact Git commit ID. Build failures remain failing GitHub jobs while their diagnostic artifacts are still uploaded.

The aggregate `Build/CMake/NightlyCompare.cmake` report compares matching configurations to the last successful nightly whose results remain available. It reports compilation-time and executable-size changes, lost tests, and new warnings in the GitHub step summary. Compilation timings are observational because GitHub runner variance and cache hits can dominate small builds. Artifacts are retained for 30 days (raw metrics), 21 days (logs and packages), or 60 days (consolidated regression reports). There is no Python dependency in the nightly workflow.

The full nightly workflow also runs automatically when its own source/scripts are modified on `develop`, providing a one-time regression check of workflow changes.

CI outcomes are recorded from real workflow runs; a successful native bootstrap does not imply editor, graphics or terrain functionality.

# 10 - Build and dependency contract

**Status:** native C++17 bootstrap implemented; editor and runtime ports pending.

## Source and build organization

- Root `CMakeLists.txt` owns the native C++17 build and CPack packaging.
- `Engine/Source/Core` builds `SIRCore` as a static library.
- `Engine/Source/Programs/Launcher` builds the `SIREngine` executable with version/help entrypoints and a minimal lifecycle smoke path.
- `Tests/Unit` provides four CTest-registered lifecycle and launcher checks.
- `Build/CMake/` contains optional dependency handling, hygiene checks and formatting rules.
- `Upstream/` contains pinned Godot, NomadFramework and Terrain3D sources for forthcoming migrations; none is linked into the bootstrap executable.
- `ThirdParty/` contains pinned EASTL, EABase and EAAssert; EASTL is opt-in through `SIR_WITH_EASTL`.

CMake **4.4.5** is the required build-system release; C++17 is mandatory and compiler extensions are disabled. FASTBuild 1.20 executes the primary CMake presets, with persistent incremental dependency tracking, a local object cache, and optional distributed workers. The Ninja `tooling` preset supplies per-file Clang compilation databases. No native build invokes Python or SCons. As long as parts of Godot's original build remain unported, their source reference still relies on its upstream SCons graph; the new native launcher does not invoke it.

## Commands

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

The same developer presets work on Windows and Linux. Other presets: `debug`, `profile`, `release-ipo`, `bulk`, `asan`, `ubsan`, and Ninja `tooling`. Configure a profile once, then rebuild incrementally. macOS remains an experimental manual build target.

## CI/CD

`.github/workflows/ci.yml` runs repository hygiene, pinned clang-format and native Debug compiler/link/CTest gates on Linux and Windows. The formatting job downloads and verifies the pinned native clang-format executable directly, without invoking Python or a package manager. A dedicated action installs CMake and FASTBuild on the build runners.

`.github/workflows/nightly.yml` compiles Release builds on both platforms, runs CTest, packages the executable and uploads retained artifacts. FASTBuild's native compilation cache uses distinct platform/configuration namespaces. Nightly also runs profiling and Linux sanitizers; the Ninja tooling job exports `compile_commands.json`. The cache stores object outputs, not relocated dependency databases. macOS can be enabled manually.

No assertion of successful remote CI is implied by committed workflows; acceptance is based on actual GitHub job results.

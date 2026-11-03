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

CMake **4.4.5** is the required build-system release; C++17 is mandatory and compiler extensions are disabled. Ninja is used by the provided presets. Foundation builds require no Python package installer or SCons. As long as parts of Godot's original build remain unported, their source reference still relies on its upstream SCons graph; the new native launcher does not invoke it.

## Commands

```sh
git clone --branch develop https://github.com/SIGAAMDAD/SIREngine.git
cd SIREngine
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci

cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
cmake --build --preset linux-release --target package
```

Use the Windows presets on Windows; macOS remains an experimental manual build target.

## CI/CD

`.github/workflows/ci.yml` runs repository hygiene, pinned clang-format and native Debug compiler/link/CTest gates on Linux and Windows. It installs CMake with a dedicated GitHub Action rather than a Python package. The source-format job currently invokes `python3 -m pip` only to obtain the pinned clang-format binary; the CMake build and native test jobs do not invoke Python.

`.github/workflows/nightly.yml` compiles Release builds on both platforms, runs CTest, packages the executable and uploads retained artifacts. Compiler caching uses `mozilla-actions/sccache-action` and distinct platform/configuration cache namespaces. The cache stores compiled outputs, while fresh CMake configuration and test runs remain source-specific. macOS can be enabled manually.

No assertion of successful remote CI is implied by committed workflows; acceptance is based on actual GitHub job results.

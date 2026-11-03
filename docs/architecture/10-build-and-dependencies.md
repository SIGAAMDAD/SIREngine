# 10 — Build system, toolchains and dependency contract

**Status:** foundation implemented; full native CMake migration for Godot is not implemented.

## Baseline tools (2026-11-02)

- CMake **4.4.5** is the pinned configuration entrypoint.
- C++ **17** is mandatory for SIREngine-owned code, with compiler extensions disabled.
- SCons **4.11.1** is pinned for the inherited Godot source build.
- clang-format **22.1.5** is the pinned SIR-owned C++ formatter.
- Python 3 is required; Ninja is the default CMake generator in CI.
- EASTL, EABase, EAAssert and Godot are pinned to exact Git commit SHAs by submodules (see `.gitmodules` and `git ls-tree HEAD`).

## Source graph

`CMakeLists.txt` builds the SIR core alias target and tests. `cmake/SIRDependencies.cmake` builds local EASTL and redirects its EABase dependency to the checked-out submodule. The CMake policy minimum is explicitly raised to 3.5 to accept EABase's historical 3.1 minimum under modern CMake. `cmake/SIRAudioDependencies.cmake` declares external licensed/system audio SDKs. `cmake/SIRGodot.cmake` provides the pinned editor build through SCons.

## Reproducible commands

```sh
git submodule update --init --recursive
python -m pip install "cmake==4.4.5" "scons==4.11.1"
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci

# With Godot's platform dependencies installed:
cmake --preset linux-godot
cmake --build --preset linux-godot
```

On Windows use corresponding `windows-ci` and `windows-godot` presets. Install the D3D12 dependencies using the pinned Godot source's `misc/scripts/install_d3d12_sdk_windows.py` before a Windows editor build. CMake currently orchestrates rather than replaces Godot's SCons source graph.

## CI and evidence

On every push to `develop` and pull request into `develop`/`main`, GitHub Actions runs tracked-file hygiene and formatting plus Windows/Linux C++17 foundation builds. At 07:17 UTC each day, the same workflow also compiles the pinned Godot editor for Windows and Linux and publishes 7-day executable artifacts if the builds succeed. A manually invoked workflow can include an experimental macOS foundation build. No results are claimed before the workflows actually run.

Generated intermediates must remain ignored. CMake exports a compilation database for SIR-owned translation units; Godot compilation closure must be obtained from actual SCons configs rather than inferred from the CMake database.

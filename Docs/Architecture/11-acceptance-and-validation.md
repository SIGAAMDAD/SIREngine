# 11 - Validation and acceptance

The bootstrap's acceptance gates and future engine integration gates are distinct.

| Gate | Evidence required |
| --- | --- |
| Source and dependencies | Exact Git tree, pinned upstream SHAs, tracked-path audit |
| C++17 native build | Real object compilation and linker results for Core and Launcher |
| Unit/runtime smoke | CTest lifecycle, version, help and invalid-option behavior |
| Formatting | Repository-pinned clang-format check of owned C++ sources |
| Package | Native ZIP package generated from the Release build |
| Godot/editor port | Editor launch, scene import/save and platform initialization |
| Terrain3D port | Brush, region serialization, collision, material and rendering tests |
| GRDG | Typed handles, publication, reclaim, concurrency and sanitizer tests |
| Graphics/audio | Real backend initialization and resource-lifetime tests |
| Memory | Domain allocation tracing, failure handling, peak and reclaim evidence |

`Build/CMake/CIHygiene.cmake` validates the tracked source tree and pinned submodules; `Build/CMake/CIFormat.cmake` invokes the pinned source formatter. Linux/Windows native Debug builds run on push and pull request. Nightly native Release builds run on both platforms, with package artifacts and compiler caching. The inherited Godot, Terrain3D and NomadFramework repositories are retained for source audits and phased porting rather than compiled as a substitute for first-party SIREngine functionality.

Report distinct states for compiled, linked, tested, blocked and not run. A C++17 bootstrap running does not prove editor startup, integrated terrain or GRDG. Every migration milestone requires its own source-closed tests.

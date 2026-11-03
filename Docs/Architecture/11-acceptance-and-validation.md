# 11 - Validation and acceptance

The C++17 foundation and the engine migration have separate acceptance gates.

| Gate | Evidence required |
| --- | --- |
| Source authority | Exact first-party tree SHA, external source provenance for imported code, tracked-path hygiene |
| Native build | Compiler and linker output for Core, Launcher and new registered systems |
| Unit/runtime smoke | CTest lifecycle, version, help and invalid-option tests |
| Formatting and source contracts | Pinned clang-format, owned-header/enum checks, and Clang AST semantic checks for C++ changes |
| Packaging | Release archives generated from a successful native build |
| Startup/editor | Native editor launch, scene import/save, inspection and shutdown |
| Terrain | Built-in terrain authoring, region persistence, materials, collision, streaming and rendering |
| GRDG | Typed IDs, publication, safe reclamation, concurrency and sanitizer tests |
| Graphics/audio | Actual backend/device initialization and resource lifetime tests |
| Memory | Allocation-domain tracing, failure behavior, high-water and reclamation evidence |

`Build/CMake/CIHygiene.cmake` checks the first-party source tree and pinned external library gitlinks. `Build/CMake/CIFormat.cmake` validates owned sources against the pinned formatter. `Build/CMake/CIStyle.cmake` checks the mechanical global-scope and SIREngine header rules; Clang AST audits cover semantic details beyond the mechanical gate. Push/PR jobs compile and test the Windows/Linux native foundation; nightly jobs build Release configurations, run tests and package artifacts. Milestone-specific acceptance expands as systems are implemented.

Compilation and runtime results must reflect the exact source/configuration under test. The native Core bootstrap is not evidence for a functional editor, Terrain system, or shared resource graph.

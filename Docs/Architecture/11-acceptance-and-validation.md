# 11 - Validation and acceptance

The C++17 foundation and the engine migration have separate acceptance gates.

| Gate | Evidence required |
| --- | --- |
| Source authority | Exact first-party tree SHA, external source provenance for imported code, tracked-path hygiene |
| Native build | Compiler and linker output for Core, Launcher and new registered systems |
| Unit/runtime smoke | CTest lifecycle, launcher tests and optional GoogleTest assertions | 
| CPU profiling | Google Benchmark JSON with source SHA, repeated median timing and platform-labelled nightly comparisons |
| Formatting and source contracts | Pinned clang-format, owned-header/enum checks, and Clang AST semantic checks for C++ changes |
| Packaging | Release archives generated from a successful native build |
| Startup/editor | Native editor launch, scene import/save, inspection and shutdown |
| Terrain | Built-in terrain authoring, region persistence, materials, collision, streaming and rendering |
| GRDG | Typed IDs, publication, safe reclamation, concurrency and sanitizer tests |
| Graphics/audio | Actual backend/device initialization and resource lifetime tests |
| Memory | Allocation-domain tracing, failure behavior, high-water and reclamation evidence |

Changed C++ source is checked with the pinned clang-format 23.1.5 formatter. CMake and CTest exercise the native targets on Windows and Linux. Full source qualification uses LLVM 23.1.5 and real instrumented production code; deeper architecture/ABI inspection is performed when relevant to the changed subsystem.

Compilation and runtime results must reflect the exact source/configuration under test. The native Core bootstrap is not evidence for a functional editor, Terrain system, or shared resource graph.

## Interim LLVM source coverage requirement (November 2026)

For the present integration phase, native C++ subsystem acceptance requires **at least 90% LLVM line coverage, 90% branch coverage, 90% function coverage, and 90% region coverage**, evaluated independently against the declared compiled source scope. This replaces the previous 100% subsystem coverage gate until the owner raises the threshold. It does not change the requirement for passing assertions, native builds, sanitizers, formatting, or integration contracts.

Coverage evidence must come from the **exact candidate commit and compiler configuration**. Missing source mappings, empty metric denominators, skipped required tests, and stale LLVM profiles fail qualification; never count them as 100% coverage. Include both implementation and compiled header scope where applicable.

The canonical runner performs an instrumented CMake/Ninja build, executes registered native tests, merges real LLVM profiles and checks all four metrics against the compiled production source inventory:

```sh
SIR_QUALIFICATION_SCOPE=all SIR_COVERAGE_MINIMUM=90 python3 Build/Scripts/RunLLVM23Qualification.py
```

Release promotion uses the separately required 100% threshold. A missing source mapping or a skipped test is an incomplete qualification, not a prerequisite for independent test development.

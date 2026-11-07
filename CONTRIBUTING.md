# Contributing to SIREngine

SIREngine is a C++17 engine built with CMake 4.4.5 and FASTBuild 1.20. See [the C++ standard](Docs/Standards/cpp.md) and [architecture](Docs/README.md).

## Workflow

1. Make a focused change on a branch from current `develop`. Obtain owner approval before changing shared APIs, ownership, threading, serialization or architectural contracts.
2. Format changed C++ with clang-format 23.1.5. Run `cmake --preset dev`, `cmake --build --preset dev` and the relevant `ctest --preset dev -R <pattern>` cases.
3. Commit a coherent change and open a PR into `develop`. CI builds and tests Linux GCC, Linux Clang and Windows MSVC.
4. Use full LLVM 23 source coverage, sanitizers, native backend tests and benchmarks at qualification and release boundaries. The PR does not need to reach engine-wide coverage before tests can be written.

Do not commit SDK redistributables, generated binaries, secrets or unattributed third-party code. SIREngine-owned code uses the global namespace and the repository's source formatting conventions.

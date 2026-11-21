---
name: SIR build system
description: Apply build, compiler, dependency and preset conventions.
applyTo: 'CMakeLists.txt,**/CMakeLists.txt,CMakePresets.json,Build/**,.github/workflows/**,Tools/**/*.cmake'
---
# Build rules

- Treat CMake 4.4.5, FASTBuild normal-development presets and existing dependency policy as baseline; do not silently substitute Ninja for production builds.
- `dev` is the normal FASTBuild flow. `tooling` is the Ninja compile-database flow; coverage and sanitizer presets are separate qualification tools. Check whether a preset exists before executing it.
- Current repository main sets C++17 in `CMakeLists.txt` and `CMakePresets.json`. The desired future primary standard is C++23 with C++17 fallback. A standard migration must change relevant CMake/compiler/test/header contracts as one explicit task.
- Keep external SDK options feature-gated. Do not bundle vendor redistributables, generated build artifacts, license-restricted files or credentials.
- Build only requested configurations plus the minimum necessary regression targets; qualify across supported compilers and backends when task scope requires it.
- Formatting is pinned clang-format 23.1.5; review actual CI hook and affected files before enforcing a new workflow-wide gate.

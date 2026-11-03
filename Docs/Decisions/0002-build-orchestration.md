# ADR-0002 - Native CMake build and dependency boundaries

- **Date:** 2026-11-02
- **Status:** accepted

CMake 4.4.5 is the entrypoint for the first-party C++17 engine, unit tests, packaging, and build configurations. FASTBuild is the primary compilation backend in the current presets. Ninja is available for tooling and compilation-database generation. The foundation does not require Python or SCons.

Imported engine subsystems are added as real CMake targets in the first-party source hierarchy, with explicit dependencies, generated-source rules and platform/compiler configurations. Terrain is a built-in runtime system and will receive native target registrations as part of its implementation milestone.

Third-party runtime libraries are kept under `ThirdParty/` or resolved through supported SDK discovery. The CI matrix retains Windows/Linux native Debug and Release gates, formatting, compiler caching and independent tests. A cache hit is not proof of runtime correctness.

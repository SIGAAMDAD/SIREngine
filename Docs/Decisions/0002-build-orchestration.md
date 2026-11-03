# ADR-0002 - Native CMake targets with separately pinned source inputs

- Date: 2026-11-02
- Status: active

CMake 4.4.5 is the entrypoint for the first-party SIREngine C++17 application, core libraries, native tests and packages. External engine source is kept as pinned Git references under `Upstream/` until ported into native CMake targets. The inherited SCons build is not invoked by the bootstrap and remains available only within the Godot source reference for source comparisons.

The dependency graph is explicit: Core builds independently, the Launcher links Core, and optional library adapters enter the build only when needed. Platform-specific editor, graphics, terrain and other subsystems are introduced at their corresponding milestones, with compile/link and behavioral evidence. Native Linux/Windows Debug and Release configurations run in CI; compiler outputs are cached without treating a cache hit as a passed test.

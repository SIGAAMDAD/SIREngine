# Changelog

This changelog records notable version-controlled changes to SIREngine.

## Unreleased

### Source foundation
- Established the general-purpose native C++17 engine layout under `Engine/Source/` with buildable Core and Launcher targets.
- Separated `Upstream/` source-port references from first-party engine source and pinned Godot, NomadFramework and Terrain3D commits.
- Reorganized documentation into `Docs/` and established four implementation milestones.
- Removed the unused experimental `SIRConstants.h`, `SIRContainers.h` and their placeholder smoke test.
- Replaced Python-heavy CI bootstrap with native CMake/CTest, cached compiler outputs via sccache, and nightly Windows/Linux Release packaging.

### Prior development history
- Introduced the initial Godot-derived branch foundation and architectural contracts.
- Established the original C++17 build scaffolding, EASTL-family dependencies, coding standards and community files.

Implementations are recorded as complete only once their native source and acceptance tests exist.

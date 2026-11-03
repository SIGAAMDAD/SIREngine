# Changelog

## Unreleased

- Established the native C++17 engine source tree, Core/Launcher targets and CTest foundation.
- Organized terrain as a built-in runtime system with a dedicated integrated editor boundary.
- Removed external engine-source checkout directories and their Git submodule declarations.
- Removed the unused experimental constants and container-alias headers.
- Established four staged implementation milestones: framework services, editor/startup, Terrain and GRDG.
- Configured native CMake and FASTBuild workflows with compiler caching.

An implementation milestone is complete only when corresponding engine source and tests exist.

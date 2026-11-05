# ADR-0003 - Flat PascalCase subsystem source layout

- **Date:** 2026-11-02
- **Status:** accepted and applied to the native bootstrap

## Decision

Use a single `Source/` root and direct subsystem directories instead of a large `Engine/Source/Runtime/...` hierarchy. All first-party source directory names and C++ filenames use PascalCase. Files are colocated with their owning implementation and tests reference exported include paths; external vendor directory names are preserved.

The source layout reserves direct ownership for the following systems, with directories introduced when genuine source or integration work exists: `Core/`, `System/`, `Engine/`, `Scene/`, `Resources/`, `Renderer/`, `Terrain/`, `World/`, `Mesh/`, `Physics/`, `Navigation/`, `Animation/`, `Audio/`, `Scripting/`, `Networking/`, `UI/` and `Editor/`. Terrain is a built-in runtime subsystem, with specialized editor code in `Source/Terrain/Editor/`.

No artificial `Runtime/`, `Public/`, `Private/`, `Programs/`, `Developer/` or `Modules/` source layers are introduced. A source directory is an ownership boundary, not automatically a separate DLL or CMake target. Unimplemented systems do not need README-only filesystem placeholders.

## Native build

The bootstrap is now compiled from:

```text
Source/
├── CMakeLists.txt
├── Core/
│   ├── CMakeLists.txt
│   ├── SIRApplication.h
│   └── SIRApplication.cpp
├── Main.cpp
├── Terrain/
│   └── Editor/
└── [additional subsystem directories]
```

The root CMake entrypoint builds `Source/`, which builds the existing `SIRCore` library and `SIREngine` application. The public Core include directory is `Source/`; call sites use `<Core/SIRApplication.h>`. External SDK discovery, FASTBuild caching, tests, packaging and C++17 toolchain settings are retained.

Tests stay under `Tests/`; the two dependency smoke filenames also follow PascalCase. CI hygiene and formatting check the new source root and reject unexpected top-level directories.

## Constraints

- The native Core/Launcher bootstrap must preserve its behavior, existing tests and binary/install target names.
- Terrain, editor, GRDG, physics, rendering and C# scripting require source and runtime verification before being declared implemented.
- Subsystems should use a small number of meaningful CMake targets; header/implementation separation by visibility must be justified by actual consumers.
- All future portable and platform-specific code follows [C++ standards](../Standards/cpp.md).
- Existing historical source paths remain available in Git history, not in the live source layout.

This is a layout and build-path migration; no broad runtime or ABI rename was part of the change.

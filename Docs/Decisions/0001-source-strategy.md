# ADR-0001 - First-party engine ownership

- **Date:** 2026-11-02
- **Status:** accepted, revised for the flat source hierarchy

SIREngine's own code resides under `Source/` in direct, PascalCase subsystem directories. Each subsystem owns its declarations, definitions, lifetime contracts and local build dependencies. The portable foundation resides in `Source/Core/`, platform integration in `Source/System/`, graphics in `Source/Renderer/`, and the built-in terrain system in `Source/Terrain/`.

General editor infrastructure belongs in `Source/Editor/`; terrain-specific authoring belongs under `Source/Terrain/Editor/`. `Source/Main.cpp` is the native application entrypoint. External libraries remain under `ThirdParty/` while original engine/framework sources are consulted during first-party migration without nested engine checkouts.

See [ADR-0003](0003-flat-source-tree.md) for the dependency and build rules adopted during the source relocation.

This decision preserves the C++17/FASTBuild-based native foundation. It does not indicate completion of the editor, terrain implementation or resource graph.

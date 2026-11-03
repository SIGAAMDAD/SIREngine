# 13 - Native engine migration sequence

**Status:** C++17 Core/Launcher bootstrap implemented; four integration milestones scheduled.

First-party engine implementation is kept under `Engine/Source/`. Required third-party *libraries* live under `ThirdParty/`; source revisions and archives consulted for migration are validated separately before selected code is integrated into the engine. Terrain is part of the built-in runtime, not a detachable extension.

1. **[Port NomadFramework C++ services](../Milestones/01-NomadFramework.md)** — portable Core utilities, allocation services, and reusable framework infrastructure with verified source and licensing.
2. **[Port Godot-derived startup and editor](../Milestones/02-Godot-Startup-Editor.md)** — engine lifecycle, registration, scene and editor capabilities, native build graph, and Windows/Linux editor acceptance.
3. **[Integrate the built-in Terrain system](../Milestones/03-Terrain.md)** — region storage, materials, brush and erosion tooling, clipmap/LOD streaming, collision and editor/runtime parity.
4. **[Begin the GRDG](../Milestones/04-GRDG.md)** — canonical resource identities, single-writer publication, multiple-reader access, dependency metadata, and tested reclamation.

Each integration slice includes real source, CMake registration, API/ABI compatibility where applicable, allocation and ownership evidence, and executable tests. Remove superseded code only after its consumers are migrated. Central resource ownership remains a cross-cutting concern through all milestones.

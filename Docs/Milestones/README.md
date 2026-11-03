# Implementation milestones

The four milestones are ordered by major dependencies. Milestones describe real source integration and acceptance, not directory creation.

| Order | Milestone | Status |
| --- | --- | --- |
| 1 | [Port NomadFramework C++ systems to SIREngine](01-NomadFramework.md) | Planned |
| 2 | [Port Godot engine startup and editor functionality to SIREngine](02-Godot-Startup-Editor.md) | Planned |
| 3 | [Integrate the built-in SIREngine Terrain system](03-Terrain.md) | Planned |
| 4 | [Begin development of the GRDG](04-GRDG.md) | Planned |

The native Core and Launcher are a working bootstrap for these milestones. Terrain belongs permanently to `Engine/Source/Runtime/Terrain/` and its tools to `Engine/Source/Editor/Terrain/`.

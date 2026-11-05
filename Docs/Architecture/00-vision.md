# 00 - SIREngine vision and objectives

**Status:** architectural direction; native implementation is at the bootstrap stage.

SIREngine is an open-source, general-purpose game engine derived from Godot. It aims to combine an approachable editor and productive content workflow with a native runtime built for predictable performance, compact data representations, centralized resource ownership, portability, and observable memory use. The engine supports both 2D and 3D applications; no particular game, rendering budget, art style, or genre is part of its core contract.

The project is organized as reusable engine infrastructure, not a collection of game-specific systems. Runtime components may be replaced or extended without disturbing unrelated authoring capabilities or forcing a single middleware provider. Efficiency is an engineering goal to be demonstrated with representative workloads and measurements, not a declared result of code style or language selection.

## Operating principles

1. Keep the authoring environment usable throughout the migration. Do not discard editor serialization, scene inspection, or import workflows.
2. Define explicit ownership, lifetime, error and allocation policies for engine subsystems, including external middleware.
3. Separate editable scene representations from cooked, compact runtime data when a workload benefits from it.
4. Reuse authoritative resource identities and dependency information across systems. Minimize duplicated CPU-owned asset data.
5. Implement native C++17 code with source-grounded performance and lifetime verification.
6. Support multiple titles and independent memory/resource-budget configurations.
7. Build and test the engine incrementally on Windows and Linux; macOS follows the initial platforms.

## Current architecture

`Source/` contains SIREngine-owned C++17 systems, organized directly by subsystem. Terrain is built in at `Source/Terrain/`, with its specialized authoring code under `Source/Terrain/Editor/`. Shared editor code lives in `Source/Editor/`, and platform services in `Source/System/`. `ThirdParty/` is reserved for external libraries. The current native executable implements the lifecycle bootstrap, with editor and other runtime systems still to be ported.

The [milestone index](../Milestones/README.md) specifies the next four engineering stages.

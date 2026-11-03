# Milestone 2 - Port Godot startup and editor functionality

**Status:** planned.

Port the Godot-derived application lifecycle, platform entrypoints, object/type registration, resource loading, scene construction and integrated editor into SIREngine's first-party source tree. Preserve useful editor behavior and asset workflow contracts while adapting internal ownership and the build graph.

## Deliverables

1. Source/ABI inventory of application startup, class registration, scene tree, editor modules, serialization and generated build steps, verified against the selected Godot source revision.
2. Native CMake targets for the application runtime and editor, with required dependency and generated-source rules.
3. Windows/Linux editor startup, scene import, save/load, inspector, tool invocation, and shutdown tests.
4. Source-level attribution, copyright and license preservation for the implementation brought into this repository.

**Exit criteria:** the SIREngine editor starts, imports and saves representative content, and shuts down correctly using the native build system. The existing Core/Launcher bootstrap is not the editor implementation.

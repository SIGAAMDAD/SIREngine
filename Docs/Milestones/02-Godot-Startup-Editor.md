# Milestone 2 — Port Godot engine startup and editor functionality to SIREngine

**Status:** not started. **Source reference:** `Upstream/Godot` at the pinned fork commit.

Port the essential platform entrypoints, main-loop initialization, object/type registration, resource loading, scene construction and editor application plumbing into the SIREngine source tree. The retained Godot-derived editor workflow must be implemented as native SIREngine CMake targets. Startup, tool invocation and error handling must remain explicit and testable.

## Deliverables

1. Source/ABI inventory of startup, application lifecycle, object/class database, scene tree, editor modules, and generated build artifacts.
2. Independently built native engine and editor targets registered through CMake, with correct dependencies and generated source rules.
3. Editor boot, scene load/save, import, inspection and shutdown acceptance tests on Windows and Linux.
4. A documented replacement for each imported source/build responsibility and preservation of applicable upstream copyright and license notices.

**Exit gate:** a native SIREngine editor can launch and execute representative authoring workflows using the SIREngine build system. The first-stage `SIRApplication` bootstrap is infrastructure, not completion of this milestone.

# Milestone 1 — Port NomadFramework C++ systems to SIREngine

**Status:** not started. **Source reference:** `Upstream/NomadFramework` and the supplied `nframework.zip` archive when its exact contents can be reconciled.

Build the reusable C/C++ foundation as first-party SIREngine modules. Inventory and compare the reference implementations for core platform types, logging, filesystem, events, configuration, console and any allocator code present in the supplied archive. Choose explicit source ownership and allocate suitable CMake targets instead of copying an entire unrelated framework blindly.

## Deliverables

1. Source-origin and licensing manifest identifying exact implementations to port and excluded gameplay-specific code.
2. Portable `Engine/Source/Core/` services with C++17 interfaces, unit tests and clearly named initialization/shutdown operations.
3. Memory allocation domain integration designed to support the later resource graph, with traced ownership and failure paths.
4. Windows and Linux compilation, link, tests and source-bound formatting evidence.

**Exit gate:** the selected C++ systems compile and pass native tests independently of a Godot editor or third-party engine runtime. Source import is audited against the supplied archive and pinned repository before acceptance.

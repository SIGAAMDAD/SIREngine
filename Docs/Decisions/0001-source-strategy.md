# ADR-0001 - Pinned upstream source references during native porting

- Date: 2026-11-02
- Status: active during staged migration

The new SIREngine source layout is organized independently of the pinned Godot fork. `Upstream/Godot`, `Upstream/NomadFramework` and `Upstream/Terrain3D` carry exact Git revisions as source references for staged integration. `Engine/Source/` and `Modules/` contain the new engine-owned code and module ports.

This replaces the earlier `engine/godot` CMake/SCons bridge layout. The original sources and licensing remain traceable through Git history and the current submodule revisions. The first-party application cannot claim to be a fully ported Godot runtime until the editor/startup milestone passes native integration gates.

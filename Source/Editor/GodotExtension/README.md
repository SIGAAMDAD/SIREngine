# Godot 4.8 native in-process authoring adapter

`SIREditorGodotSession` wraps the existing fixed-width `SIREditorNativeBridge`
API as an editor-only `godot::RefCounted` class registered at the SCENE API
initialization level. The adapter is intentionally kept separate from the
native `SIREditor` executable, Godot's scene serializer and shipping runtime.

Each session is single-authoring-thread owned. It initializes one bounded
editor-domain `SIRMemoryManager` and one scene workspace, opens one saved
`.tscn`, copies source metadata into Godot `Dictionary` / `Array` values, then
closes and destructs the native workspace before releasing the memory manager.
It never returns a borrowed source pointer or lets a scene-generation handle
escape into GDScript. UTF-8 text spans larger than 8 KiB fail the snapshot;
large serialized property values are shown as an explicit placeholder, with
the true serialized length retained. The existing Inspector is authoritative
for editing and saving values.

The bridge supports at most 2048 nodes and 8192 properties in this transition;
Godot's `Tree` control in this test dock is not yet virtualized. The adapter
returns the same `schema: 1` dictionaries as the JSON bridge so that the
EditorDock's validation/selection code remains unchanged.

Configure with `SIR_EDITOR_ENABLE_GDEXTENSION=ON` and an externally supplied
version-matched `godot-cpp` checkout and extension API. CMake builds the optional
GDExtension using native C++17, with no Godot SCons step in SIREngine. Use
`cmake --install ... --component EditorGodotBridge --prefix <Godot project>`
to stage the library and descriptor. Real fork SDK compilation, loading,
GUI interaction, and process-lifetime profiling require a compatible editor
binary and were not executed in the default native test configuration.

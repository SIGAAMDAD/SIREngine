# Godot 4.8 editor dock bridge (migration test)

Open `Tools/GodotEditorBridge` as a project in the supplied Godot 4.8 fork. Its `EditorDock` plugin is enabled in `project.godot`.

Build the native SIREngine `SIREditor` executable first. In the **SIREngine Scene** dock, enter the absolute path to that executable, open and save `Scenes/BridgeSmoke.tscn` (or another `.tscn` scene) in the Godot editor, and press **Refresh**. The dock requests `--inspect-scene-json` and shows the native hierarchy using Godot's actual `Tree` control. Clicking a row selects the corresponding Godot scene `Node` in the existing inspector. The plugin is editor-only, does not change the scene or write resource files, and clears stale snapshots when the scene changes or saves.

The bridge is a transitional integration and smoke-test surface for Godot `EditorDock`, `Control`, `Tree`, `EditorSelection`, and native SIREngine scene identity. It is not yet the standalone SIREngine graphical editor kernel. It uses an explicitly requested blocking native inspection of **saved** text scenes, caps the display at 2048 nodes and does not claim virtualized Godot Tree rendering. The native virtualized hierarchy model will replace this preview when the full editor-private Godot kernel and direct native widget bridge are hosted by SIREngine.

`--inspect-scene-json` schema v1 contains string-valued 64-bit IDs, source-order node records, decoded relative NodePaths, line numbers and resource index metadata. It never returns Godot Variant payloads or serialized property values.

The Godot plugin has been source-checked against the uploaded Godot fork API (`EditorPlugin.add_dock`, `EditorDock`, `EditorInterface`, `Tree`, `OS.execute`). Its runtime smoke test requires the Godot editor executable and a GUI session; the native CMake/CTest suite cannot certify that integration without those prerequisites.


## Nonblocking inspection (migration phase)

**Refresh** now invokes `OS.create_process` with `--inspect-scene-json-file`,
so the main editor event loop remains responsive during native inspection.
The native executable writes a sibling `.tmp` staging file and renames it to
its destination only after a complete JSON document has been flushed and
closed. The dock polls process completion rather than capturing stdout.

The dock cancels active inspections when the editor switches or saves a scene,
when another inspection starts, or when the plugin unloads. It discards
results if the selected scene or the saved file's SHA-256 has changed. There
is a 30-second process timeout and a 4-MiB JSON display cap. Output files
live in the Godot project's `user://` cache area and are removed after
consumption or cancellation. This remains a saved-file, read-only bridge;
there is no scene/Variant runtime mirroring or editor-history mutation.

Native file-mode acceptance is exercised by `editor.host.json-file-unit` and
`editor.host.json-file-cli` CTest cases. Godot GUI process lifecycle and dock
selection still require a real fork editor smoke test.

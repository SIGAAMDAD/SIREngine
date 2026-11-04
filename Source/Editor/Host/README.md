# Native editor scene host

`SIREditorSceneWorkspace` joins the native `.tscn` scanner, resource dependency
index, transactional document, and virtualized scene-tree model in one editor
memory domain. It is intended to be the scene-data boundary consumed by a
future Godot-derived `Control`/`Tree` presenter.

- The caller supplies an `SIR_MEMORY_DOMAIN_EDITOR` segment. All retained source
  bytes and model buffers use the segment's `SIRLinearAllocator`, including
  history and hierarchy scratch. There is no fallback allocator.
- `openTextSource` copies the source, preserving borrowed offset validity;
  `openTextFile` uses C stdio to read the same representation.
- `close()` destroys the document/view and rewinds allocations; another scene
  can then be opened in the same workspace.
- Parse errors fail closed and report the source line. Unsupported Godot
  scene inheritance/instances remain explicitly rejected.
- Scene source remains read-only: property and resource serialization is the
  responsibility of the Godot editor-private ResourceLoader/ResourceSaver.
- The host does not create Nodes, instantiate Godot Controls, or own GPU/GRDG
  resources. The native `SIREditor --inspect-scene` executable provides a
  testable startup-to-scene-intake path, not the graphical editor.

The scene intake is limited to 32 MiB of source data per open operation, with
larger or out-of-budget projects rejected instead of allocating outside the
editor memory domain. This is an initial host input limit, not a runtime budget.

## Asynchronous Godot dock transport

`SIREditor --inspect-scene-json-file <source.tscn> <result.json>` publishes
schema-v1 JSON to an explicitly supplied file, using a sibling `.tmp` staging
path and a final rename only after a successful flush and close. Invalid
workspace state, an unreadable destination or an output path identical to the
source returns a nonzero status. No intermediate JSON is presented as a
successful snapshot.

The Godot 4.8 editor-only bridge launches this command using `OS.create_process`
and polls `OS.is_process_running` without blocking the editor event loop. The
source is rehashed before acceptance and stale/timeout results are discarded.
The transport is a bounded saved-file migration adapter, not a live native
Godot object loader or a replacement for the eventual editor-private API.

## Source-backed serialized property index

`SIREditorSceneWorkspace` now indexes the property declarations belonging to
`[node]` sections of a saved Godot `.tscn`. It uses a measure-then-fill
scanner with caller-owned editor-domain storage and balanced quoted/nested
values (including multiline strings and dictionaries). Each entry refers to
its owning native node ID, original property name/value spans and source line.
These are **read-only serialized text spans**, not Godot `Variant` values or
validated PropertyInfo metadata. Other section types, inherited scene
resolution, editing, property conversion and saving remain with Godot's editor.

The existing scene JSON schema `1` adds an optional `properties` collection.
Each element carries `node`, `name`, `value`, `line`, `bytes`, and `truncated`.
The transport displays at most 512 bytes of a serialized value per entry,
without cutting a UTF-8 codepoint. Full serialized bytes remain in the source
workspace. Existing `nodes` and `resources` fields are unchanged.

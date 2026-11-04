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

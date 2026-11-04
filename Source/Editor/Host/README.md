# Native SIREngine editor scene workspace

`SIREditorSceneWorkspace` owns one source-scene intake, revisioned
`SIREditorSceneDocument`, resource/property source indexes, and virtualized
hierarchy, all backed by a caller-supplied `SIR_MEMORY_DOMAIN_EDITOR` segment.

The `.tscn` scanner is a **format importer**, not the canonical editable
property representation. `openTextSource` copies and retains source bytes
so borrowed offsets remain valid; `close` destroys the view and document
and rewinds editor-domain arena storage.

Imported metadata is read-only until native typed property and resource
serialization is connected. Inherited/instanced scenes currently fail
explicitly. The native `SIREditor --inspect-scene` command is a headless
diagnostic, not a graphical editor.

Source intake currently limits authored scene files to 32 MiB per open.
That limit and editor commit budgets are implementation inputs rather than
runtime game memory budgets.

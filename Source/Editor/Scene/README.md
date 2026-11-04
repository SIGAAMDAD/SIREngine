
## Node property metadata (read-only)

`SIREditorTextProperties` uses the validated text-scene node header index to
borrow source spans of serialized node property keys and raw values. It
recognizes strings and escaped quotes, balanced `()`, `[]`, `{}` nesting,
and multiline values without interpreting or allocating Variant objects.
Call `sirEditorScanNodeTextProperties` with null output and zero capacity to
measure required storage, then supply exactly that much editor-domain memory
and scan again. Error paths publish zero entries. The 65536-record bound and
64-level nesting bound prevent uncontrolled parser work. Source text and
result storage must be kept alive by the same authoring workspace.

## Native typed scalar editing and source roundtrip

`SIREditorSceneDocument` now journals typed property mutations in the same
atomic revision and undo/redo history as authored node edits. Supported
properties are validated through the existing SIRObjectRegistry metadata.
The editor alone owns mutable authored state; the runtime SIRObjectStore
remains a distinct generation-safe object lifetime service.

`SIREditorScalarCodec` provides bounded, allocation-free conversion of
boolean, signed/unsigned 64-bit, and finite 64-bit real scalars to/from
Godot text-scene syntax. `SIREditorScalarBinding` maps explicitly supplied
(node ID, property ID, source property index) bindings into one native
document transaction, then can regenerate only those scalar spans while
preserving all unrelated source bytes. The caller supplies every scratch
buffer and serialized output buffer. Invalid bindings/values fail without
partially committing authoring state.

**Outstanding R2/R3:** actual Godot-derived method dispatch/signals, string
atom and mathematical property codecs, automatic reflection/property binding,
ResourceLoader/ResourceSaver, full Godot scene/Variant serialization and
inherited/instanced scene resolution, and complete scene-inheritance resolution.

## Native scene save/reopen

`SIREditorFileSave` stages serialized scene bytes into an exclusively created
sibling path supplied by the caller. The Linux implementation writes, syncs,
renames and synchronizes the parent directory, preserving regular-file
permissions; Windows uses UTF-8 to wide-path conversion, CreateFileW,
FlushFileBuffers and MoveFileExW with replace/write-through semantics. The
function distinguishes failure before replacement from replacement committed
without confirmed directory durability. A failed save does not mutate the
canonical editor document or the original source buffer.

`sirEditorSaveBoundScene` exports the bounded supported scalar subset from
the native editor journal into caller-owned scratch, then publishes it through
the file writer. For subsequent edits, close and reopen the workspace to
reconcile the new serialized spans. Fixture tests validate real disk
save/reopen and verify that scalar edits preserve untouched serialized
strings, resources, vector expressions and connection blocks.

The writer does not claim full Godot resource serialization, inherited-scene
save semantics or editor UI integration.

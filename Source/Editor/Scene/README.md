# Godot text-scene intake — native authoring bridge

The `sirEditorScanTextScene` scanner is a bounded **read-only** front end for
Godot 4.8 `.tscn` **node hierarchy headers**. It provides source locations,
node paths, Godot `unique_id`, stable local IDs and resolved parent/type links.
`sirEditorReconcileTextScene` publishes an index to the editor-only native
`SIREditorSceneDocument` in one validated revision/undo transaction.

The parser is independently authored against the pinned fork's
`scene/resources/resource_format_text.cpp` `_parse_node_tag` and `_parse_nodes`
behavior. It does not parse properties, resolve external resources, reconstruct
inherited scenes or serialize Godot scenes. Those remain with the Godot
editor-private authoring kernel and its `VariantParser`/`ResourceLoaderText`.
Any instantiated node is rejected with `SIR_EDITOR_TEXT_UNSUPPORTED_INSTANCE`
in this scanner; full import must use the Godot-owned resource loader.

The caller supplies the source bytes, node-index storage, path arena and editor
document transaction buffers, backed by the SIR editor memory domain. The
scanner does not allocate, create worker threads or mutate the source.
Failed scans publish zero indexed nodes; failed reconciliations cancel the
transaction and preserve the previous live revision. A valid scan requires
parent-before-child node header order, as produced by Godot's text scene saver.

The native bridge is designed for editor hierarchy indexing, previews and
source diagnostics while porting the full Godot 4.8 UI and serializer. It is
**not** an alternate shipping scene format, runtime Node subsystem, asset
cooker or replacement UI.

Regression tests include two actual scene files from the pinned fork and
synthetic malformed/capacity/reconciliation stress cases.

## External and embedded resource dependency index

`sirEditorScanTextResources` indexes Godot `[ext_resource]` and
`[sub_resource]` declarations as source-backed metadata, preserving their
text-scene ID, type, external path, optional UID, line number, and deterministic
per-scene key. It validates `ExtResource("id")` / `SubResource("id")` calls
against the corresponding declarations, including forward references, without
allocating memory or resolving Godot `Variant` values. The ID namespaces are
separate, matching the serialized expression kinds. Missing references,
duplicate declarations, malformed calls, and insufficient caller-owned buffers
produce source-line diagnostics and clear the published index.

Consumers must keep the source buffer alive while interpreting offsets and
combine the per-scene stable key with the canonical scene identity. This
resource index is intended for editor dependency diagnostics and as input to
the future importer/cooker. The Godot `ResourceLoaderText` remains responsible
for loading, interpreting and saving actual property/resource objects. This
scanner does not rewrite scenes, instantiate objects, or replace that loader.

# Native object/type foundation (R2)

`SIRObjectRegistry` provides caller-owned type/property descriptors with
inherited property lookup and strict scalar value-kind checking. Descriptor
names are IDs referring to the single engine string atom pool, not new
owned strings. Registration is closed by `seal()`.

`SIRObjectStore` provides generation-checked native object handles with
explicit allocation and release in caller-owned slots. It is a runtime
lifetime primitive; the existing `SIREditorSceneDocument` remains the sole
writer of authored scene objects. The editor authoring layer now uses these descriptors to validate typed scalar
properties in SIREditorSceneDocument's single atomic node/property journal,
with bounded caller-owned property slots, undo/redo, and deletion cleanup.
Native method dispatch, signals, script binding, and math/string atom conversion
are not yet ported.

The implementation is an adaptation of Godot's object/type-registration
concepts into SIREngine-owned IDs and memory, not a port of its `Object` or
`ClassDB` source text. Actual method dispatch, signals, editor property
metadata and script binding remain pending.

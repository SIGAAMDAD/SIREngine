# Native editor hierarchy projection

`SIREditorHierarchyModel` projects immutable snapshots from `SIREditorSceneDocument`
into visible rows with stable authored IDs. It prepares the scene tree for the
Godot-derived `Control`/`Tree` host without requiring runtime Godot objects in
SIREngine's native editor authoring library.

- The editor supplies disjoint storage from `SIR_MEMORY_DOMAIN_EDITOR`; neither
  the model nor the list performs heap allocations.
- Stable row state (expansion and variable row height) is carried forward by
  authored ID when a new document revision is rebuilt. A removed selection
  is cleared; stale operations require a refresh.
- Children are sorted by parent and ordinal, then enumerated with an explicit
  traversal stack. Root rows begin expanded. Other nodes begin collapsed.
- `sirEditorHierarchyOrderFromTextScene` derives per-sibling source order from
  validated `.tscn` node line numbers. Rebuilds accept that table sorted by ID.
  This preserves text-scene authored ordering even though native snapshots are
  sorted by stable identifier.
- The existing `SIREditorVirtualList` Fenwick tree resolves the visible index
  range from a scroll position plus overscan. The Godot dock creates Control
  rows only for that range when the full editor shell is integrated.
- Layout and paint epochs are separate: changing selection only repaints;
  expanding/collapsing or changing a visible row height invalidates layout.
- This is a SIREngine-owned implementation of the *algorithmic* concepts behind
  UE5 Slate's list virtualization and invalidation. No Unreal code is included.

**Lifetime:** only the editor main thread mutates this view. A UI consumer must
call `isCurrent(document)` before using cached rows, and must discard borrowed
row data when the layout epoch changes. Neither `SIREditorHierarchyModel` nor
its snapshot is a cross-thread GRDG lease.

**Limits:** this is an editor-side view model, not a full Godot scene importer,
serialized-property parser, `Control` widget, viewport, or node runtime. The
Godot editor private authoring kernel is a subsequent migration stage.

The text-scene intake now resolves Godot `parent="."` to the scene root
node ID rather than encoding it as a second root; this is essential for a
correct visible hierarchy projection.

The transitional Godot EditorDock exposes the native read-only source-backed
node properties in a second two-column Tree. This is separate from the full
Godot Inspector's PropertyInfo/Variant editing infrastructure, which remains
to be migrated into the standalone graphical SIREditor.

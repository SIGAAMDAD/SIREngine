# In-process native editor bridge (ABI v1)

`SIREditorNativeBridge` exposes the existing editor authoring workspace through
C-linkage functions without serializing JSON or launching another process. The
header exposes only PODs, fixed-width enums, and an opaque handle; no Godot
`Object`, `Node`, `Variant`, `RID`, resource ownership, or STL type crosses it.

The host supplies correctly aligned **storage** for the small bridge object,
using `sirEditorNativeBridgeStorageBytes()` and
`sirEditorNativeBridgeStorageAlignment()`. It also supplies its already
initialized `SIRMemorySegment` from the engine's **single** `SIRMemoryManager`,
with domain `SIR_MEMORY_DOMAIN_EDITOR`. The bridge borrows that segment and
initializes one `SIREditorSceneWorkspace`. There is no second memory manager,
OS reservation, or general heap owner. The host controls its normal segment
reservation and commit budgets. Host storage must not move until
`sirEditorNativeBridgeDestroy()`, and the segment must outlive the bridge.

1. Call `sirEditorNativeBridgeInit(storage, bytes, editorSegment, &handle)`, then `OpenFile` or `OpenText` on the editor
   authoring thread. A failed open leaves the session closed.
2. Acquire `sirEditorBridgeSnapshot_t` to obtain the ABI version, counts,
   document revision, and **generation token**.
3. Use `NodeAt`, `ResourceAt`, `PropertyAt` to copy metadata, and `CopyField`
   with that generation to copy a source-backed string into a caller-owned
   buffer. The required size includes NUL. A too-small buffer is untouched.
4. `Close()` or a subsequent reopen changes the generation; cached handles
   from earlier snapshots return `SIR_EDITOR_BRIDGE_STALE`.
5. Destroy the bridge before releasing the host storage.

The interface is single-writer/single-thread and deliberately read-only; it
is not a resource serializer or a concurrent snapshot lease. The Godot editor
remains the authoritative owner of `ResourceLoader`/`ResourceSaver`, live
`SceneTree` and Inspector mutations. Godot-derived UI host bindings should
call this C-linkage adapter within the same process, convert to editor UI
models, and enforce generation checks before presenting results. **The current
GDScript `EditorDock` still uses the existing asynchronous helper process.**
A native GDExtension/module adapter and actual graphical runtime acceptance
are separate milestones, not implied by this static library and its CTest.


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

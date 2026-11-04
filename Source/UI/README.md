# Native retained UI foundation (R4)

`SIRControlTree` is a SIREngine-owned retained control hierarchy using
caller-owned slots and an explicit stack. It supports generation-checked
handles, parent/child ownership, absolute and vertical stack layout,
clipping, visibility, pointer hit testing, focused key routing, and
separate layout/paint invalidation epochs. It uses screen-space pixel
rectangles, not a duplicate vector/math library.

These semantics were selected from the supplied Godot `Control` source and
adapted to the native SIREngine memory and engine contracts. This is only
the headless control/input substrate; text shaping, themes, accessibility,
full dock layout, platform windows, draw generation, and the graphical
SIREditor shell are not implemented in this slice. Those remain R4 work.

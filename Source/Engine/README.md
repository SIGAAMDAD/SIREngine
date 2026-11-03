# Engine lifecycle

The eventual engine initialization, subsystem registration, main-loop dispatch and shutdown ordering are owned here. The initial `SIRApplication` bootstrap remains in `Source/Core/` until the lifecycle port makes this an executable engine module.

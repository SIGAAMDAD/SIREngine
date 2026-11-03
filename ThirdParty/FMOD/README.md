# FMOD Studio API SDK

FMOD is an external, licensed SDK dependency. This directory contains **no FMOD binaries or source distribution**.

Install the FMOD Studio API separately and configure SIREngine with:

```sh
cmake --preset tooling -DSIR_WITH_FMOD=ON -DSIR_FMOD_SDK_ROOT=/path/to/fmod/studio/api
```

The root must contain `api/core/inc/fmod.h`, `api/studio/inc/fmod_studio.hpp`, and platform-appropriate Core and Studio link libraries under `api/core/lib/` and `api/studio/lib/`.

CMake exposes `SIR::FMODCore` and `SIR::FMODStudio` when discovery succeeds. Native audio provider code and runtime loading tests are separate implementation work. Supply matching FMOD runtime libraries through the platform's packaging/deployment process.

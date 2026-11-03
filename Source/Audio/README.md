# Audio

The runtime audio interface will support FMOD Studio and OpenAL Soft providers. Licensed FMOD binaries remain outside the repository; OpenAL Soft is a pinned optional third-party source dependency.

CMake declares `SIR::FMODStudio`, `SIR::FMODCore` and `SIR::OpenAL` dependency boundaries when enabled. Real provider implementation, event/bank loading and allocation tracking remain future work.

See [audio architecture](../../Docs/Architecture/06-audio-backends.md) and [dependency setup](../../Docs/Dependencies.md).

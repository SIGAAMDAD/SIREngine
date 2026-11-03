# 06 - Audio provider architecture

**Status:** optional OpenAL Soft source build and FMOD SDK link targets are declared. Engine audio provider code is pending.

The runtime-facing audio interface will own events, voices, listener state, routing and audio-resource handles without leaking middleware SDK types into the portable Core.

| Provider | Dependency | Integration plan |
| --- | --- | --- |
| FMOD Studio | External, licensed Studio API SDK | Initial provider; CMake exposes `SIR::FMODCore` and `SIR::FMODStudio` |
| OpenAL Soft | Pinned and locally compiled open-source implementation | Native OpenAL provider; CMake exposes `SIR::OpenAL` |
| Wwise | External, licensed SDK | Future provider; SDK headers are declared |
| System OpenAL | Installed OpenAL implementation | Alternative to OpenAL Soft, via `SIR_WITH_OPENAL` |

FMOD is configured using `SIR_FMOD_SDK_ROOT`. OpenAL Soft is enabled with `SIR_WITH_OPENAL_SOFT=ON`; `SIR_WITH_OPENAL=ON` chooses a system installation instead. The latter two cannot both be selected.

Provider-specific event and voice metadata remain under the public audio API's ownership. FMOD memory callbacks must be routed through the engine audio allocation domain before runtime acceptance. Bank streaming, decoded buffers, DSP state and other middleware allocations must be traced. Derived audio bytes should register their resource identity with GRDG, while callbacks avoid unbounded allocations and blocking I/O.

Provider acceptance requires actual playback/event loading on Windows/Linux, device loss and shutdown testing, callback allocation measurement, and license-compliant packaging. Declared CMake targets are dependency wiring, not completed providers.

See [dependency configuration](../Dependencies.md).

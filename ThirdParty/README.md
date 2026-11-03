# Third-party dependencies

Only external libraries belong in this directory. The first-party runtime remains under `Source/`.

| Component | Acquisition | CMake option | Current integration |
| --- | --- | --- | --- |
| EASTL, EABase, EAAssert | Pinned Git submodules | `SIR_WITH_EASTL` | Optional container source |
| OpenAL Soft | Pinned `kcat/openal-soft` source | `SIR_WITH_OPENAL_SOFT` | Native library target `SIR::OpenAL` |
| meshoptimizer | Pinned `zeux/meshoptimizer` source | `SIR_WITH_MESHOPTIMIZER` | Native target `SIR::MeshOptimizer` |
| FMOD Studio API | Separately installed SDK | `SIR_WITH_FMOD` | Core/Studio SDK link targets |
| Mono | Separately installed embedding SDK | `SIR_WITH_MONO` | Runtime SDK discovery and link target |
| Wwise | Separately installed SDK | `SIR_WITH_WWISE` | Header discovery for future provider |

Use `git submodule update --init ThirdParty/OpenALSoft ThirdParty/meshoptimizer` only if these sources are enabled. Default engine startup builds do not initialize or build the optional libraries. No FMOD or Mono binary distributions are committed here.

See [dependency contracts](../Docs/Dependencies.md) for pinned source identities, SDK location variables and validation commands.

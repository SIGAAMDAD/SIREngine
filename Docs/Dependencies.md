# Dependency acquisition and build contracts

**Status:** pinned library sources and optional CMake SDK targets established. The full engine audio, geometry and C# scripting adapters are pending.

## Dependency matrix

| Component | Source or SDK | Pinned revision / resolution | Build switch | Provided target |
| --- | --- | --- | --- | --- |
| OpenAL Soft | [kcat/openal-soft](https://github.com/kcat/openal-soft) | `89b70ceb71bf58753853a7ec5495aff0c7c5b5af` | `SIR_WITH_OPENAL_SOFT` | `SIR::OpenAL` |
| meshoptimizer | [zeux/meshoptimizer](https://github.com/zeux/meshoptimizer) | `f0a684f11ead064e176775c9b796f82a55910e92` | `SIR_WITH_MESHOPTIMIZER` | `SIR::MeshOptimizer` |
| FMOD Studio API | Proprietary, installed SDK | `SIR_FMOD_SDK_ROOT` | `SIR_WITH_FMOD` | `SIR::FMODCore`, `SIR::FMODStudio` |
| Mono | Installed Mono embedding SDK | pkg-config or `SIR_MONO_SDK_ROOT` | `SIR_WITH_MONO` | `SIR::MonoRuntime` |
| OpenAL (system fallback) | OS package | `find_package(OpenAL)` | `SIR_WITH_OPENAL` | `SIR::OpenAL` |
| Wwise | Proprietary, installed SDK | `SIR_WWISE_SDK_ROOT` | `SIR_WITH_WWISE` | Header interface |
| EASTL family | Electronic Arts source | `git ls-tree HEAD ThirdParty/` | `SIR_WITH_EASTL` | EASTL |

## Building OpenAL Soft and meshoptimizer

```sh
git submodule update --init ThirdParty/OpenALSoft ThirdParty/meshoptimizer
cmake --preset tooling -DSIR_WITH_OPENAL_SOFT=ON -DSIR_WITH_MESHOPTIMIZER=ON
cmake --build --preset tooling
ctest --preset tooling --output-on-failure
```

The two optional native dependency smoke tests compile and link the actual vendor libraries when these switches are enabled. Vendor test programs, demo executables and ancillary utilities are disabled.

OpenAL Soft and a system-installed OpenAL library are mutually exclusive selections for the same `SIR::OpenAL` abstraction. CMake fails if both switches are set.

## External SDK discovery

FMOD uses `SIR_FMOD_SDK_ROOT` and requires matching Core and Studio headers and link libraries. Mono uses `SIR_MONO_SDK_ROOT` or compatible pkg-config development packages, and requires a native embedding header/library. The dependency targets do not implement audio event playback or C# script execution. Those systems need engine-owned source and platform runtime tests.

The selected licensed SDKs are installed outside Git. Pin a validated version in release-specific build manifests when the actual providers become build dependencies.

## CI

Normal push/PR builds compile the minimal SIREngine foundation without requiring large optional libraries or licensed SDKs. Dependency-enabled configurations are checked separately; source availability, configuration, compilation and runtime acceptance are distinct gates. Keep compiler caches scoped to platform, toolchain, build profile and dependency revision.

## Additional pinned third-party source imports

The [`ThirdParty/IMPORT_LOCK.json`](../ThirdParty/IMPORT_LOCK.json) manifest records **44 new pinned source gitlinks** and their verified upstream commit SHAs. `.gitmodules` registers the corresponding fetch URLs. These are source acquisitions only: there are **no new always-on CMake targets**, successful vendor builds, runtime adapters, or performance claims in this change.

```sh
git submodule update --init --recursive
```

### Version policy

- `fastgltf` is locked to tag `v0.9.1` (requested 0.9 series).
- `libpng` is locked to a `libpng16` branch snapshot.
- All other new sources are locked to verified upstream branch-head SHAs at import, **not** floating branches.
- Steamworks is obtained separately through Valve and is not vendored.
- OpenGL and Vulkan function loading remain engine-owned: do not add GLAD or volk.
- Shipping any enabled dependency requires its own build/link/runtime and licensing acceptance, including transitive codec libraries or SDK components.

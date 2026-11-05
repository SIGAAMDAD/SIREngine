# Dependency acquisition and build contracts

**Status:** pinned library sources and optional CMake SDK targets established. The full engine audio, geometry and C# scripting adapters are pending.

## Dependency matrix

| Component | Source or SDK | Pinned revision / resolution | Build switch | Provided target |
| --- | --- | --- | --- | --- |
| GoogleTest | [google/googletest](https://github.com/google/googletest) | `a8380b01a25ebdb130097b607b3afed2a58d04ba` | `SIR_WITH_GOOGLETEST` | `gtest_main` |
| Google Benchmark | [google/benchmark](https://github.com/google/benchmark) | `1813ad9c3a828cee126f399739443c329e75bc79` | `SIR_WITH_GOOGLE_BENCHMARK` + `SIR_BUILD_BENCHMARKS` | `benchmark::benchmark_main` |
| OpenAL Soft | [kcat/openal-soft](https://github.com/kcat/openal-soft) | `89b70ceb71bf58753853a7ec5495aff0c7c5b5af` | `SIR_WITH_OPENAL_SOFT` | `SIR::OpenAL` |
| meshoptimizer | [zeux/meshoptimizer](https://github.com/zeux/meshoptimizer) | `f0a684f11ead064e176775c9b796f82a55910e92` | `SIR_WITH_MESHOPTIMIZER` | `SIR::MeshOptimizer` |
| FMOD Studio API | Proprietary, installed SDK | `SIR_FMOD_SDK_ROOT` | `SIR_WITH_FMOD` | `SIR::FMODCore`, `SIR::FMODStudio` |
| Mono | Installed Mono embedding SDK | pkg-config or `SIR_MONO_SDK_ROOT` | `SIR_WITH_MONO` | `SIR::MonoRuntime` |
| OpenAL (system fallback) | OS package | `find_package(OpenAL)` | `SIR_WITH_OPENAL` | `SIR::OpenAL` |
| Wwise | Proprietary, installed SDK | `SIR_WWISE_SDK_ROOT` | `SIR_WITH_WWISE` | Header interface |
| EASTL family | Electronic Arts source | `git ls-tree HEAD ThirdParty/` | `SIR_WITH_EASTL` | EASTL |

## Google testing and profiling

GoogleTest provides first-party unit tests under `Tests/Unit/`. Google Benchmark provides CPU microbenchmarks under `Tests/Benchmarks/`; both are opt-in dependencies so normal engine builds do not link testing or profiling libraries. Existing CTest launcher/lifecycle tests remain registered alongside GoogleTest when it is enabled.

```sh
git submodule update --init ThirdParty/GoogleTest ThirdParty/GoogleBenchmark
cmake --preset profile -DSIR_WITH_GOOGLETEST=ON -DSIR_WITH_GOOGLE_BENCHMARK=ON -DSIR_BUILD_BENCHMARKS=ON
cmake --build --preset profile
ctest --preset profile --output-on-failure
bash Build/Scripts/RunGoogleBenchmarks.sh local-profile
```

The benchmark script writes Google Benchmark JSON into `out/Nightly/Benchmarks/`. The scheduled Windows/Linux profiling jobs retain that data and compare the median CPU times with the previous successful nightly, while normal CI compiles and runs the new GoogleTest cases on both platforms. These initial benchmarks cover application lifecycle and tick overhead, not an entire game frame. Measurements on shared CI runners are trend observations, not calibrated CPU performance counters or deterministic pass/fail performance limits.

The underlying GoogleTest and Google Benchmark libraries retain their BSD-3-Clause and Apache-2.0 licenses respectively. Only the two pinned Git submodules are added; no floating downloads or Python dependencies are required during CMake configuration.

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

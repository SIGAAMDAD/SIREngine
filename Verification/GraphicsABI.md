# Shared graphics ABI v1 — test and acceptance registry

**Source owner:** `Source/Renderer` on `renderer/shared-driver-api-v1`. **Contract:** major 1, minor 0.
**Independent verification manifest:** `Verification/Systems/GraphicsABI.json`.
**CTest scope:** `^renderer[.]graphics[.]`. Native Vulkan/D3D12/GL implementation is outside this branch.

## Registered CPU tests

| CTest entry | Acceptance responsibility |
| --- | --- |
| `renderer.graphics.interface` | C++ registry, feature masks, validation and selection ordering |
| `renderer.graphics.binary-abi` | C function-table negotiation, size/version/host mismatch and device caps |
| `renderer.graphics.pipeline-eras` | Modern, programmable, shaderless fixed-function model admission |
| `renderer.graphics.pipeline-bridge` | C++ descriptors to fixed-width ABI representation, transactionality |
| `renderer.graphics.binary-module` | Dynamically loaded mock DLL/DSO: adapters, resource creation, models, command submission, WSI, retirement and device loss |
| `renderer.graphics.contract` | ABI 1.0 compile-time layout, version invariants, malformed feature/format records |
| `renderer.graphics.properties` | 4,096 deterministic descriptor mutation rounds with reproducible seed and fatal assertions |
| `renderer.graphics.negative-matrix` | Malformed adapter/capability/format/model limits, invalid source pipelines and shader-stage requests |
| `renderer.graphics.era-matrix` | Cross-era feature and stage/profile/combiner/light restrictions |
| `renderer.graphics.gtest.*` | Independently discoverable GoogleTest assertion cases for ABI invariants and failure branches |

The deterministic test executables do not require a GPU, OS window, software rasterizer, or native Vulkan/D3D12 driver.

## Running

Standard developer checks:

```sh
git submodule update --init ThirdParty/GoogleTest
cmake --preset dev -DSIR_WITH_GOOGLETEST=ON
cmake --build --preset dev --target SIRGraphicsInterfaceTests SIRGraphicsABITests SIRGraphicsPipelineABITests SIRGraphicsPipelineBridgeTests SIRGraphicsABIMockModule SIRGraphicsModuleTests SIRGraphicsABIContractTests SIRGraphicsABIPropertyTests SIRGraphicsABIEraMatrixTests SIRGraphicsABIAssertionGoogleTests
ctest --preset dev --output-on-failure --no-tests=error -R '^renderer[.]graphics[.]'
```

Linux sanitizers: configure/build/test with `--preset asan` and `--preset ubsan`, keeping the instrumented binaries separate. The GitHub workflow `.github/workflows/graphics-abi.yml` runs the same CPU test set under Linux/Windows, separate sanitizers, pinned formatting and an LLVM coverage run whenever the ABI branch is pushed.

LLVM coverage:

```sh
git submodule update --init ThirdParty/GoogleTest
SIR_GRAPHICS_ABI_MIN_COVERAGE=90 bash Build/Scripts/RunGraphicsABICoverage.sh
```

The script uses one Clang/LLVM 18 instrumentation toolchain, a dedicated Ninja build, fresh profraw files, and `llvm-profdata-18`/`llvm-cov-18`. `CheckGraphicsABICoverage.py` requires all six production renderer translation units to appear and reports covered/total for line, branch, function and region metrics. No empty denominator or missing source can pass. The normal project gate remains 90%; independent full qualification requires 100% of each counter. Neither is claimed from unexecuted tests.

## Native backend obligations — BLOCKED until backend adoption

| Contract ID | Native evidence required |
| --- | --- |
| `graphics.vulkan.headless` | Vulkan 1.4 device, resources, command submit/poll/retire and device loss |
| `graphics.vulkan.software` | Real lavapipe adapter classification and supported shader/format/queue behavior |
| `graphics.d3d12.hardware` | Direct3D 12 GPU resource, PSO, queue and DXGI swapchain integration |
| `graphics.d3d12.warp` | Real WARP adapter selection and image output |
| `graphics.opengl.modern` | OpenGL 4.6 stateful modern path with serial queue behavior |
| `graphics.programmable.legacy` | D3D9/10/11 or GL2.1 shader profiles, limited slots, forward path, negative compute requests |
| `graphics.fixed.legacy` | GL1.x / D3D7-8 fixed transform, lights, texture combiners, unsupported shader requests |
| `graphics.device.recovery` | Recreated device generation, stale handles, queue fence and surface acquisition invalidation |
| `graphics.image.oracles` | Real target frame image tests for each admitted renderer era, not mock signatures |

Native contracts require exact GPU/adapter, OS, driver, compiler, binary/source SHA, feature mask, test commands, outputs and recovery traces. GPU image tests compare documented tolerance or semantic content appropriate to each era; bitwise parity is not assumed.

## Qualification rules

- Tests must execute and pass on the exact reviewed source SHA. A registered CTest name alone is not evidence.
- Windows and Linux CI, sanitizer, source-format, ABI layout and full scoped coverage must be checked independently.
- Mock DLL/DSO tests establish host contract behavior only. They cannot certify native GPU queues, WSI, software rendering, physical hardware, driver loss or visual correctness.
- The production source inventory is all six `Source/Renderer/*.cpp`; no exclusions, no stale merged coverage profiles and no zero-denominator success.
- Outstanding native contracts and benchmarks remain **NOT RUN** until implementing driver branches are integrated with the shared ABI.

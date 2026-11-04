# SIREngine renderer pipeline eras — one graphics ABI v1

**Status:** shared binary ABI v1 pipeline-model types, device discovery, tagged recipes and portable validation implemented; no native three-era renderer has been executed. **Authority:** owner directive on 2026-11-04 to establish modern, programmable and very-legacy pipeline models before further shared driver changes; the shared graphics ABI remains **v1.0** during foundation work.

**Tracked by:** [SIR-ABI-v1 #130](https://github.com/SIGAAMDAD/SIREngine/issues/130); [native convergence #125](https://github.com/SIGAAMDAD/SIREngine/issues/125); [shader program #77](https://github.com/SIGAAMDAD/SIREngine/issues/77) and [legacy shader qualification #89](https://github.com/SIGAAMDAD/SIREngine/issues/89). **Owned branch:** `renderer/shared-driver-api-v1`.

## Source-backed starting point and gaps

- `SIRGraphicsTypes.h` already defines per-API backend IDs, feature masks, `SIR_GRAPHICS_PIPELINE_GRAPHICS`, `SIR_GRAPHICS_PIPELINE_COMPUTE`, `SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION`, shader payload formats, states and generation-scoped 16-byte handles.
- `SIRGraphicsDriverABI.h`, `SIRGraphicsABIOperations.h`, and `SIRGraphicsPipelineABI.cpp` now expose the model-specific device callback, model-tagged pipeline recipes, modern/programmable shader profiles, and shaderless fixed-function lights/materials/stages with portable negative validation.
- `sirGraphicsSelectABIProfile` currently returns headless-compute/reduced-graphics/modern-graphics levels (0/1/2). It must remain distinct from historical pipeline-model support.
- The native D3D12 and Vulkan paths live on separate branches. OpenGL and D3D7–11 are not integrated into the shared branch. No three-era native execution or image parity is proven.

## Architecture — orthogonal axes, not one simulated GPU

```text
Renderer / material authoring / FrameGraph pass intent
                  |
     Capability + Pipeline Model Resolver  [planned]
         /                 |                \
   MODERN MODEL      PROGRAMMABLE MODEL    FIXED MODEL
   PSO + bindings    bounded shader/state  T&L + stages
         \                 |                /
     One common SIRGraphics module ABI v1
                  |
     Native backends, queues, memory and WSI
        VK | D3D12 | GL | legacy D3D
```

The **pipeline model** (modern, programmable, fixed) governs material representation, shader availability, state and draw prerequisites. The independent **execution model** (explicit resource/queue management vs serial stateful recording/replay) governs backend scheduling. The independent **adapter class** (discrete/integrated/virtual/CPU software) governs device admission. A backend may expose multiple pipeline models. Do not infer one of these axes from another.

| Native API / adapter capability | Advertised model(s) after real probing | Execution behavior |
| --- | --- | --- |
| Vulkan 1.4+, D3D12 | Modern | Explicit descriptors, command encoding and sync |
| OpenGL 4.6 | Modern when required features pass; programmable otherwise | Stateful context, serial DIRECT; cache programs/state; no fake GPU queues |
| D3D11 | Programmable; modern-capable only when the concrete device passes the modern recipe | Driver-managed state objects and command lowering |
| D3D10 | Programmable | Bounded VS/PS/GS where supported; state objects |
| D3D9 | Programmable; optional fixed-function if native provider reports it | Legacy shader profiles and stateful device |
| D3D8 | Fixed-function **and/or** limited programmable model | VS/PS 1.x only when reported; stateful device |
| D3D7 | Fixed-function | Fixed transform/lighting and texture stages |
| OpenGL 2.1 | Programmable; fixed-function if compatibility state is present | GLSL 1.x programs and/or fixed state |
| OpenGL 3.x | Programmable or modern-qualified; FFP only in supported compatibility context | Context-specific feature table; core profile has no FFP |
| OpenGL 1.3 | Fixed-function | Fixed vertex/fragment state and texture combiners |

This table lists *candidates* and limiting expectations, never automatic promises. GL3.1 removed its original fixed-function core interfaces; later compatibility contexts may restore them. D3D8 supports both fixed function and early programmable shader models. Explicit selection remains strict about backend choice.

## Pipeline contracts to freeze in v1

### Shared admission and identity

Implemented global types: `sirGraphicsPipelineModel_t` (modern, programmable, fixed-function), `sirGraphicsExecutionModel_t` (explicit, stateful-serial), and `sirGraphicsABIPipelineModelCaps_t`. `sirGraphicsABIPipeline_t` contains `nModel` and mutually exclusive pointers to the concrete `sirGraphicsABIModernRecipe_t`, `sirGraphicsABIProgrammableRecipe_t` and `sirGraphicsABIFixedRecipe_t`. A separate `sirGraphicsABIPipelineRecipe_t` wrapper is not used.

The v1 device shall enumerate supported pipeline models and query caps per model before pipeline creation. A size-tagged binary capability record shall include:
- Model bitset and valid execution mode(s), graphics/present queue masks, supported shader stages and corresponding shader language/bytecode profiles; do not confuse shader-stage availability with API version.
- Maximum texture/image units, active material/texture stages, vertex streams/attributes, varying/interpolator limits, uniform/constant slots, shader and attachment counts, render-target formats/sample counts and buffer limits.
- Fixed material operation support: maximum lights, alpha-test and fog modes, texture stages, stage sources/operations, vertex transformation and lighting support, texture-coordinate generation and clipping support, and explicit CPU T&L eligibility.
- NPOT and mip/anisotropic restrictions, precision/format capabilities and render-target/depth compatibility, with actual enabled-feature probes.
- Whether a particular pass can be executed on the model without compute, storage images, indirect draws or modern descriptor sets.

A model's existence is **not** permission to use every optional mode. Missing data or an impossible combination rejects admission; no optimistic defaults. Each device can advertise multiple models with disjoint limits.

The v1 pipeline-creation request must gain a **model tag** with three mutually exclusive, size-tagged recipes: modern immutable PSO/binding layout; programmable shader+bounded slot/state bundle; fixed-function materials/lighting/fog/texture combiners. It must preserve the existing common raster, vertex layout, depth, stencil, blend, target format and sample semantics. The binary surface never contains C++ `bool`, native SDK types, STL/EASTL, virtual classes or uninitialized tagged-union bytes. A modern pipeline may require compute; a fixed-function pipeline may not. Pipeline hashes include the model tag and only initialized recipe fields.

### Modern model

Offline ShaderCooker outputs device/profile-specific DXIL, SPIR-V or GLSL variants, reflection and deterministic binding signatures; native Vulkan/D3D12 create immutable PSOs and native descriptors. GL4.6 may execute a modern-*capable* material/pass recipe using cached GLSL programs and tracked state, with a serial command submission model. Optional compute/indirect/mesh/ray and advanced barriers are admitted only when the device reports them. Native driver owns PSO caches, GPU memory, queues and presentation; FrameGraph owns pass/barrier scheduling.

### Programmable-era model

Offline ShaderCooker emits explicit target profiles for GLSL 1.x/2.x/3.x and legacy D3D shader profiles as supported by available compilers. Missing compiler/profile artifacts are a cooked-build error. Vertex/fragment stages, texture slots, constants, varyings, vertex layouts, blend/depth/raster and MRT are bounded by queried device limits. A shader+state-bundle cache captures device transitions; native command replay is ordered. Forward and multipass recipes must exist without modern compute, descriptor indexing or asynchronous queues. Device/API-specific shader bytecode requirements remain native-specialized.

### Very-legacy fixed-function model

Offline materials cook into compact **shaderless** fixed-function recipes: material colors and lighting, fixed hardware transform or admitted CPU T&L, fog, alpha test, stage texture operations/sources, texture-coordinate transforms, sampler stages, depth/blend/stencil and primitive layouts. The device reports exact stages, lights and supported combine operations. GL1.3 and D3D7/D3D8 native state transitions and indexed draws remain stateful/serial. Optional CPU-side vertex processing is explicitly budgeted and selected only when available. Unsupported stage/texture formats or over-limit lights/stages cause a named rejection or an explicitly authored lower-cost recipe, never silent semantic loss. No shader bytecode, compute, resource descriptor arrays or PSO assumptions are imposed.

## Render-path and asset integration

1. The material/shader cooker writes compatible variants/recipes for the requested *candidate* models and rejects unsupported builtins, stages and over-budget material complexity at cook time.
2. At device creation the model resolver reads **device-enabled** model limits and selects one compatible model per renderer/pass/material (not merely global API version). User-selected backend/API remains fixed; a change of backend is an explicit separate policy choice.
3. Renderer/FrameGraph selects a supported render-path recipe: modern GPU-driven/forward+/deferred when allowed; programmable forward or multipass with explicit bounded lights; fixed-function forward render with texture stages and optional CPU transforms. Hi-Z, GPUScene and virtual geometry integration are model-gated, not foundational draw prerequisites.
4. GRDG retains the sole logical ID owner; each native backend retains physical resources and lifetime. Material/shader library owns cooked artifact identity; frame scheduler owns pass ordering. No second resource registry or per-era allocator.
5. Shader/pipeline caching uses model + API + stable adapter/driver fingerprint + cooked recipe/hash + vertex layout + binding/stage layout + color/depth/sample targets. GL state caches and fixed bundles are separate from native modern PSO keys. Budget/cache warm policies vary by model and measured costs; no unmeasured numeric performance promise.
6. Device loss, stale generation and frame/present lifecycle obey the shared ABI. Stateful and fixed APIs may only advertise DIRECT when native queues cannot genuinely overlap. A `barrier` is translated into supported ordering/synchronization, or returns `SIR_GRAPHICS_UNSUPPORTED`; no fabricated GPU hazards or implicit CPU waits.

## Required ABI v1 discipline

The **binary module ABI is major 1 and minor 0**, and stays there throughout the foundation. No version increments are permitted simply for completing or rearranging the still-unreleased v1 contracts. Keep one entry point and one module boundary across all models. Add model-discovery and tagged-pipeline definitions to the existing *v1* shared headers before native drivers adopt them. The current coarse 0/1/2 classifier is an independent graphics-feature tier; rename/replace it only by an explicit source migration with test updates.

## Ownership and implementation order

| Gate | Exclusive primary source owner | Exact planned output | Negative acceptance |
| --- | --- | --- | --- |
| PM00 | Shared ABI | This architectural freeze, #130 roadmap, restore v1 constants/documentation | No unapproved ABI version increment; wrong era/API mapping rejected |
| PM01 | Shared ABI | `SIRGraphicsTypes.h`, `SIRGraphicsDriverABI.h/.cpp`, `SIRGraphicsABIOperations.h`, ABI mock/contract tests | Wrong tag, wrong size, nonexistent shader stage, impossible queue/lights/stages rejected |
| PM02 | ShaderCooker #77/#89 | Modern, legacy-programmable and shaderless fixed cooked artifacts/variant manifest | Missing compiler/profile or unsupported material op cannot publish invalid artifact |
| PM03 | Native Vulkan/D3D12/GL/legacy owners #125–127/#89 | Driver-specific PSO, shader+state, fixed-function lowering and truthful device reporting | Unsupported model, GL core FFP, D3D7 shaders and false async queues fail |
| PM04 | Renderer / FrameGraph #57 | Device-admitted forward/deferred/multipass model resolver, GRDG resource views | No modern compute dependency for legacy forward, no silent backend fallback |
| PM05 | Shared QA + native owners | Cross-model capability matrix, staged scenes and image oracles, native GPU output capture | D3D8 dual-mode, GL2.1 dual-mode, GL3.2 core rejection, stage overrun, device loss |

Dependency graph: **PM00 → PM01 → (PM02 + PM03) → PM04 → PM05**. Work on ABI-03 native convergence must not proceed until PM01 is frozen. Shader + native implementers may work in parallel only after PM01 and exclusive source ownership are fixed.

## Test and review gates

**Static:** C++17 builds on Linux/Windows; exact source/formatter, Clang AST policy, fixed 16-byte handle/completion, POD layout assertions, shader package/profile contracts. **CPU:** model negotiation and contradictory caps; model-tagged pipeline validation; matrix of render-path eligibility; serializer/hash reproducibility; scoped queue/completion/memory and lost-device tests. **Native GPU:** modern Vulkan/D3D12/GL4.6 first image; programmable GL2.1/3.3 and D3D9–11, D3D8 limited shader; fixed GL1.3 and D3D7/8. Record actual format/precision and scene image comparisons rather than demanding byte-for-byte identical rendering across eras.

Mandatory negative matrix: GL1.3 compute/storage/shader request; D3D7 shader request; D3D8 dual-model but unsupported shader stage; GL2.1 valid FFP plus valid GLSL; GL3.2 **core** FFP request; GL4.6 modern model with *stateful* serial execution; absent sampler unit, over-limit fixed lights/stages, unsupported texture format/NPOT, missing cooked shader/material tier, invalid queue, stale device generation, device loss/resize, capability mismatch on cached recipe.

**Status gate:** PM01 portable capability and recipe validation is implemented with CPU and dynamic-module mock tests; native Vulkan/D3D12/OpenGL/legacy exports and visual conformance are outstanding. Mock tests and C++ compilation are not native GPU acceptance evidence.

### Implemented PM01 source integration and calling contract

- `sirGraphicsABICapabilities_t::nPipelineModelMask` uses bit `(nModel - 1)`, with model enum values 1/2/3. Drivers report only device-enabled models. `sirGraphicsABIDevice_t::pQueryPipelineModelCaps` returns size-tagged, generation-tagged, requested-model-specific support and limits; `sirGraphicsQueryABIPipelineModelCaps` rejects unadvertised models, malformed responses, unsupported queues/features, and limits exceeding the device's enabled capabilities without mutating the caller's output on failure.
- `sirGraphicsValidateABIPipelineModelCaps` and `sirGraphicsValidateABIPipeline` validate execution mode, shader formats/stages, fixed-function capability flags, sample counts, rendering format/binding limits and mutually exclusive recipe tags. `sirGraphicsCreateABIPipeline` queries real model caps, validates the recipe, calls the native driver and verifies the returned typed device-generation handle. This host wrapper is the required engine-side creation entry point.
- Fixed-function recipes carry transformation matrices, bounded texture combiners, material lighting, fixed lights, fog and alpha state. Programmable recipes carry bounded shader profiles and resource-slot demands. Modern recipes carry required feature/stage masks and cooked shader/binding signatures. These records and their stage/light arrays are borrowed for the duration of `pCreatePipeline`; backend-owned persistent pipeline objects must deep-copy any persistent data.
- `Tests/Unit/SIRGraphicsPipelineABITests.cpp` covers all three model validators and negative cases. `renderer.graphics.binary-module` additionally checks discovery, invalid model generations and modern-compute pipeline creation through an independent mock DLL/DSO. CI acceptance is recorded against the exact branch SHA; these mocks are not implementation of native legacy rendering.

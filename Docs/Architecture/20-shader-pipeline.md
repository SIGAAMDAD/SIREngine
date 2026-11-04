# Shader pipeline foundation

**Status:** first CPU-side foundation, independent of a GPU, framegraph, or driver SDK.

## Contract and ownership

`Source/Renderer/SIRShaderPipeline.{h,cpp}` extends the existing `SIR::Graphics` ABI rather than defining a second device interface. Shader variants borrow cooked bytecode from their owning resource/package. `sirShaderToGraphicsCode` converts a validated variant to the existing `sirGraphicsShaderCode_t` for `SIRRenderingDevice::createPipeline`. Backend-specific pipeline creation and physical GPU allocation remain exclusively owned by the rendering device. GRDG integration is separate.

## Implemented primitives

- **Backend format selection:** Vulkan uses SPIR-V, D3D12 DXIL, and OpenGL GLSL for the initial supported targets. Unsupported APIs fail explicitly.
- **Cooked variant resolution:** one shader stage per variant. Match backend, stage, device capability mask, requested permutation, optional vendor/device IDs, and optional uber fallback. Prefer an exact specialized permutation, then the most specific supported hardware variant. The selection is stable on equal-ranked entries. Malformed bundles fail closed.
- **Cache identity:** a deterministic two-lane 128-bit index hashes explicit descriptor scalars in fixed byte order: schema/compiler versions, optimization flags, backend/format/stage, source/include/entry/definition hashes, permutation, required features and target fingerprint. The index is **not** a content-authentication digest. Any persistent cache must verify full metadata and the cooked bytecode hash before consumption, and reject incompatible schema/toolchain/device revisions.
- **Warmup:** fixed 128-item, allocation-free priority queue, duplicate key coalescing with promotion, stable tie order and capacity errors. This queue schedules keys; it does not compile on render threads, allocate GPU pipelines, or wait on GPU fences.

## Next implementation slices

1. Offline HLSL/GLSL compiler toolchain adapters, shader reflection and binding-layout verification, with GPU- and backend-specific compiler flags.
2. Cooked shader package serialization and strong content verification, including content-addressed artifacts and invalidation on toolchain, driver and schema changes.
3. PSO/material recipe assembly and resource lifetime hooks on the common rendering-device API, without duplicating native allocation ownership.
4. Pipeline cache persistence: Vulkan cache UUID/driver validation, D3D12 PSO/library compatibility, and OpenGL program-binary capability checks.
5. Shader variant budgets, specialization/uber fallback policy, thermal/cold-start warmup scheduling and telemetry, with measured CPU stalls and driver-specific regression tests.
6. Separate framegraph and GRDG integration after their contracts are implemented.

## Acceptance scope

Native no-GPU tests `renderer.shader.pipeline` validate format routing, cache-key determinism and invalidation, hardware/uber selection, negative input handling and queue capacity/order. The foundation has standalone C++17 syntax, format and sanitizer verification. Full repository CMake/CTest and hardware-backed Vulkan/D3D12/OpenGL execution remain separate gates.

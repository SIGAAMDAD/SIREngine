# SIREngine offline ShaderCooker — SHD-01

This is the offline-only HLSL source-DAG frontend and DXC invocation for the
portable shader contracts defined in `Source/Renderer/Shaders` (SHD-00).
It creates no GPU resources, does not depend on a running renderer, and does
not compile shaders at runtime.

## Build

The repository root registers `Tools/ShaderCooker/CMakeLists.txt`, producing
`SIRShaderCooker`, `SIRShaderCookFrontend`, and two CTest suites when
`SIR_BUILD_TESTS=ON`. DXC is supplied externally; the host target does not
link or bundle `dxcompiler`.

## Usage

```sh
SIRShaderCooker --root Shaders --source Test/test.hlsl --entry main \
    --backend vulkan --stage compute --compiler /abs/path/to/dxc \
    --output Build/Shaders/test.spv --define QUALITY=2
```

Use `--backend d3d12` for native DXIL; the Vulkan path requests SPIR-V with
`-fspv-target-env=vulkan1.3`. Both use shader model 6.6 and HLSL 2021.
The DXC executable must be a real **offline** build dependency. The host
fingerprints its bytes; `*.manifest` records its fingerprint, recipe version,
entry point, profile, input DAG/define hashes and emitted binary hash. Absolute
source-root paths are excluded from the generated HLSL and manifest.
The hashes identify candidate artifacts; a future content-addressed archive
must verify full artifact bytes and source metadata on lookup.

## Determinism and supported source dialect

- Source files are stored under a single canonical root. Only relative,
  literal `#include "..."` directives are currently supported. Absolute,
  angle, macro-generated and path-escaping include forms fail explicitly.
- Include dependencies are ordered and normalized; file identities use
  logical paths, never host absolute paths. CRLF/CR is normalized to LF,
  optional UTF-8 BOM is removed, and sorted unique `--define` values enter
  the cache fingerprint.
- Expansion preserves logical `#line` locations and models `#pragma once`.
  Nested decimal integer `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`,
  `#define`, `#undef` and simple `defined(...)` tests control dependency
  expansion; skipped includes do not enter the graph. Complex preprocessor
  expressions are rejected with a diagnostic rather than guessed.
  Files repeated without `#pragma once` remain repeated in the source but
  occupy one content-addressed DAG node. Compiler preprocessing still
  evaluates the retained conditionals and macros.
- Bounded at 1 MiB per source, 8 MiB aggregate input, 16 MiB expanded
  source, 256 source records, 64 nested includes and 64 defines.
- GL 4.6 and legacy Direct3D profiles are intentionally rejected until
  their separate offline cross-compilers and verified target recipes exist.
  DXIL or SPIR-V is never mislabeled as a legacy native binary.

## Acceptance state

The local source-DAG and process-invocation suites use a **mock DXC** to prove
argument handling and deterministic manifest generation; mock bytes are not
shader compilation evidence. The test host must separately run native DXC
DXIL/SPIR-V compile, reflection, identical-input byte reproducibility and
Windows/Linux integration before SHD-01 can be closed. Backend GPU/PSO
acceptance belongs to later SHD milestones.

## Native compiler acceptance

Configure CMake with an explicit `-DSIR_SHADER_DXC_EXECUTABLE=/abs/path/dxc`
to register `shader.cooker.native-dxc`; `spirv-val` and `spirv-dis` must
also be installed. The test compiles the real HLSL fixture to DXIL and
SPIR-V twice, compares binary and manifest bytes, checks DXIL resource
reflection, and verifies SPIR-V with SPIRV-Tools. The pinned Linux CI
workflow downloads the upstream DXC v1.9.2609 archive with its published
SHA-256 checksum; the standard unit tests remain compiler-independent.

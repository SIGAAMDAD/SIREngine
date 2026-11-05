# SHD-03 — Triangle-local analytic gradient compiler groundwork

**Branch:** `feature/shader-pipeline-foundation`. **Tracking:** shader roadmap #77 / SHD-03 #81.

## Implemented interfaces

- `sirMaterialTriangleUVGradients` computes perspective-correct U/V plus screen-space dU/dx, dU/dy, dV/dx, dV/dy from three screen-space vertices with per-vertex 1/w. Barycentric derivatives are calculated from the same triangle only; UV values are authored/unwrapped at triangle boundaries. A degenerate or invalid projection gives an explicit unsupported/fallback outcome.
- `sirMaterialPropagateGradients` carries value, dx, dy through bounded typed arithmetic, static switches, uniform branch selection and outputs. Multiplication uses the product rule. Nonuniform select marks derivatives invalid; texture-sample outputs are conservatively invalid for follow-on arithmetic without independent texture partials.
- `sirMaterialTriplanarGradients` projects world-space gradients into XY, XZ, and YZ terrain UVs without sampling neighboring triangles.
- `sirMaterialPlanTextureSample` computes texture-space gradient axes, anisotropy and clamped mip footprint. Discontinuous/invalid gradients select explicit LOD fallback.
- `sirMaterialEmitSampleHLSL` writes bounded, injection-resistant `Texture2D.SampleGrad` or explicit `SampleLevel` syntax using validated HLSL identifier symbols. ShaderCooker owns this host-side text emission; the runtime never compiles shaders.
- `shader.cooker.derivatives` covers affine and perspective finite-difference reference, mirrored/unwrapped triangle UVs, degenerate geometry, mip and anisotropic axes, product rule, nonuniform branch detection, terrain triplanar axes and explicit HLSL codegen/fallback.

## Remaining SHD-03 acceptance

This is a compiler-side source/test slice. Full shader-stage graph lowering, derivative-validity provenance in the final cooked manifest, real GPU reference images, coverage/AST and VG-R6 #62 integration remain separate acceptance gates. These are not implied by the CPU tests.

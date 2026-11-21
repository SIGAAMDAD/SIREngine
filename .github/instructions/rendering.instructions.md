---
name: SIR rendering and GPU systems
description: Protect renderer, driver, GPU-resource, GRDG, terrain and shader contracts.
applyTo: 'Source/Renderer/**,Source/Terrain/**,Source/Mesh/**,Tools/ShaderCooker/**,Tests/**/*Renderer*,Tests/**/*Graphics*'
---
# Rendering / resource graph

- Inspect the active driver/backend and shader compilation pipeline before editing. Distinguish current source behavior from proposed Vulkan/D3D/OpenGL functionality.
- Preserve GRDG ownership, resource transitions, submission order, synchronization, lifetime and device-loss semantics. Prove read/write dependencies and fence lifetimes for changes.
- Preserve explicit GPU/CPU memory placement, persistently mapped allocation rules, staging ownership and transient resource reuse in implemented paths.
- Do not conflate physics simulation lifetime with renderer visibility or GPU residency.
- Avoid driver-specific logic in shared graphics contracts unless the requested work is explicitly backend-specific.
- For shader and buffer changes, confirm CPU/GPU struct layouts, alignment, padding, descriptor bindings, shader variant selection and exact compiled targets.
- For optimizations record baseline workload, frame timing, CPU/GPU split, hardware/driver, and correctness. A compile-only result does not certify a GPU feature.

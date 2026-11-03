# 05 — Graphics drivers and renderer responsibilities

**Status:** target graphics architecture. The native Core/Launcher does not yet include the rendering backends; Direct3D 12, Vulkan 1.4 and OpenGL 4.6 require porting and native verification.

| API | Initial target | Required scope |
| --- | --- | --- |
| Direct3D 12 | Windows | Native rendering-device backend, driver integration and resource ownership |
| Vulkan 1.4+ | Windows/Linux | Capability negotiation, feature dispatch and future-version compatibility |
| OpenGL 4.6 | Windows/Linux | Dedicated modern OpenGL 4.6 driver, not Godot's OpenGL 3 compatibility path |
| Metal/macOS | Later | Portability plan after stable Windows/Linux integration |

Rendering backend code will be ported and adapted into the SIREngine runtime; its source lineage provides engineering reference, not a built-in driver at the current bootstrap stage. The intended OpenGL path targets 4.6 rather than the older compatibility renderer. Vulkan instance/device version and optional capabilities are negotiated at runtime by explicit supported feature tiers.

## SIR rendering responsibilities

- Use GRDG resource identity for buffers, images, materials, meshes and shader dependencies.
- Track all GPU device allocations and transient upload/download buffers as derived resource views.
- Separate persistent mesh/material payloads from frame-local command/descriptor staging.
- Compile shader variants and material tiers offline where possible, retaining a deterministic cache key and shader format version.
- Preserve editor selection, viewport tools and import preview behavior while runtime rendering shifts behind interfaces.
- Keep driver-independent frame scheduling and feature profiles; backend-specific barriers, queues and allocations live in providers.

## Required validation

Build and launch each driver on native hardware; record PCI/device, driver, API version, selected features, present mode and failure fallback. Test resize, minimized windows, swapchain recreation, device loss, pipeline cache reload and shader-format invalidation. Benchmark under identical camera/scene loads with memory and frame-time traces. A compiled driver with no launch test is build-only status.

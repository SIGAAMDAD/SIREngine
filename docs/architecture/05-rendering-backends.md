# 05 — Graphics drivers and renderer responsibilities

**Status:** platform/API objectives. Pinned Godot capabilities are inherited where present; SIR-specific Vulkan 1.4/OpenGL 4.6 contracts remain to be implemented and tested.

| API | Initial target | Required scope |
| --- | --- | --- |
| Direct3D 12 | Windows | Existing Godot RenderingDevice path; verify driver and resource ownership |
| Vulkan 1.4+ | Windows/Linux | Capability negotiation, feature dispatch and future-version compatibility |
| OpenGL 4.6 | Windows/Linux | Dedicated modern OpenGL 4.6 driver, not Godot's OpenGL 3 compatibility path |
| Metal/macOS | Later | Portability plan after stable Windows/Linux integration |

Godot already has D3D12/Vulkan RenderingDevice backends and an OpenGL compatibility renderer. The latter targets OpenGL 3.3 desktop capability; it **does not fulfill** the requested OpenGL 4.6 backend. Vulkan instance/device API version and optional features must be queried at runtime, with explicit supported feature tiers; requesting a Vulkan 1.4 loader is not proof that all physical devices support Vulkan 1.4.

## SIR rendering responsibilities

- Use GRDG resource identity for buffers, images, materials, meshes and shader dependencies.
- Track all GPU device allocations and transient upload/download buffers as derived resource views.
- Separate persistent mesh/material payloads from frame-local command/descriptor staging.
- Compile shader variants and material tiers offline where possible, retaining a deterministic cache key and shader format version.
- Preserve editor selection, viewport tools and import preview behavior while runtime rendering shifts behind interfaces.
- Keep driver-independent frame scheduling and feature profiles; backend-specific barriers, queues and allocations live in providers.

## Required validation

Build and launch each driver on native hardware; record PCI/device, driver, API version, selected features, present mode and failure fallback. Test resize, minimized windows, swapchain recreation, device loss, pipeline cache reload and shader-format invalidation. Benchmark under identical camera/scene loads with memory and frame-time traces. A compiled driver with no launch test is build-only status.

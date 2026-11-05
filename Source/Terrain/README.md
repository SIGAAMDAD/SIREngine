# Terrain

**Classification:** built-in SIREngine subsystem.
**Status:** engine source boundary established; native Terrain implementation pending.

Terrain owns heightfield and region formats, spatial LOD, clipmaps, terrain material layering, streaming and terrain-specific updates. It shares canonical resource identities through the resource system and exposes interfaces to rendering, physics and navigation.

Terrain-specific authoring belongs in [Editor](Editor/README.md). Shared editor infrastructure belongs to `Source/Editor/`; rendering-device objects remain owned by `Source/Renderer/`.

The native integration must preserve brushes, erosion, region persistence, terrain instancing, collision, virtual-texture residency and HLOD where applicable. Build targets and integration tests will be introduced alongside real code.

See [Terrain milestone](../../Docs/Milestones/03-Terrain.md).

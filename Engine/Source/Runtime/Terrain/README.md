# Terrain

**Kind:** built-in SIREngine runtime subsystem.
**Status:** source boundary established; runtime integration pending.

Terrain owns heightfield and region data, spatial LOD, clipmap/mesh interfaces, material layers, terrain streaming, instancing metadata and terrain-specific runtime updates. It manages terrain-dependent resource identities and coordinates residency with the common resource system.

The graphics backend owns GPU submissions and device objects. Physics owns collision simulation; navigation consumes cooked traversal geometry. Terrain defines the data contracts and integration surfaces for those consumers. Authoring tools and editor-only state belong in `Engine/Source/Editor/Terrain/`.

The native port must preserve brush behavior, region persistence, terrain material authoring, erosion, collision, virtual-texture residency, HLOD and streaming behavior as applicable. Native CMake targets and tests will be registered when corresponding source arrives.

See [Terrain milestone](../../../../Docs/Milestones/03-Terrain.md).

/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

# Terrain

**Classification:** built-in SIREngine subsystem.
**Status:** engine source boundary established; native Terrain implementation pending.

Terrain owns heightfield and region formats, spatial LOD, clipmaps, terrain material layering, streaming and terrain-specific updates. It shares canonical resource identities through the resource system and exposes interfaces to rendering, physics and navigation.

Terrain-specific authoring belongs in [Editor](Editor/README.md). Shared editor infrastructure belongs to `Source/Editor/`; rendering-device objects remain owned by `Source/Renderer/`.

The native integration must preserve brushes, erosion, region persistence, terrain instancing, collision, virtual-texture residency and HLOD where applicable. Build targets and integration tests will be introduced alongside real code.

See [Terrain milestone](../../Docs/Milestones/03-Terrain.md).

/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREREGIONCOOKER_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREREGIONCOOKER_H__

#include <Renderer/VirtualTexturing/SIRVirtualTextureCooker.h>

// Preserves Terrain3D's per-row worker cancellation boundary while the
// scheduler and job tokens remain owned by SIREngine. A cancelled cook must
// discard its partially written surfaces and never publish their package.
typedef bool ( *sirVirtualTextureRegionCancelFn )( void *pUser );

// Source order is row-major: northwest, north, northeast, west, center,
// east, southwest, south, southeast. Missing neighbor data clamps at center.
// Source memory, output surfaces and package bytes remain caller-owned.
typedef struct sirVirtualTextureRegionCookRequest_s
{
	sirVirtualTextureCookImage_t neighbors[9];
	uint32_t nRegionIndex;
	uint32_t nGeneration;
	uint32_t nExtent;
	uint32_t nDirtyMip0Mask;
	uint32_t nPreviousRegionIndex;
	uint32_t nPreviousExtent;
	bool bReuseSurfaces;
	sirVirtualTextureRegionCancelFn pfnShouldCancel;
	void *pCancelUser;
} sirVirtualTextureRegionCookRequest_t;

typedef struct sirVirtualTextureRegionSurfaces_s
{
	uint8_t *pAlbedo;
	uint64_t nAlbedoCapacity;
	uint8_t *pPbr;
	uint64_t nPbrCapacity;
} sirVirtualTextureRegionSurfaces_t;

typedef struct sirVirtualTextureRegionPackRequest_s
{
	sirVirtualTextureRegionCookRequest_t region;
	sirVirtualTextureRegionSurfaces_t surfaces;
	sirVirtualTextureCookAllocator_t allocator;
	bool bZstd;
} sirVirtualTextureRegionPackRequest_t;

// A region batch is the native equivalent of the donor's cold 32-region
// publication batch. Edited source and output memory are owned by Terrain.
// The supplied revision predicate is invoked before and after cooking; the
// editor must hold its publication lock while calling its final publisher.
typedef bool ( *sirVirtualTextureRegionCurrentFn )( void *pUser, uint32_t nRegionIndex, uint32_t nGeneration );

typedef struct sirVirtualTextureRegionBatchRequest_s
{
	const sirVirtualTextureRegionCookRequest_t *pRegions;
	const sirVirtualTextureRegionSurfaces_t *pSurfaces;
	sirVirtualTextureCookSource_t *pCookScratch;
	uint32_t nRegionCount;
	uint32_t nSourceCapacity;
	sirVirtualTextureCookAllocator_t allocator;
	sirVirtualTextureRegionCurrentFn pfnIsCurrent;
	void *pCurrentUser;
	bool bZstd;
} sirVirtualTextureRegionBatchRequest_t;

// Native port of Terrain3D's incremental MegaVT region pixel recipe in
// terrain_3d_incremental_megavt_cooker.cpp (colorTap/ColorSample/BuildPixels).
// The original V1 reference encoder/cooker owns all downstream format policy.
class SIRVirtualTextureRegionCooker final
{
public:
	static bool compose( const sirVirtualTextureRegionCookRequest_t& request, const sirVirtualTextureRegionSurfaces_t& surfaces ) noexcept;
	static bool cook( const sirVirtualTextureRegionPackRequest_t& request, sirVirtualTextureCookOutput_t& package ) noexcept;
	static bool cookBatch( const sirVirtualTextureRegionBatchRequest_t& request, sirVirtualTextureCookOutput_t& package ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREREGIONCOOKER_H__

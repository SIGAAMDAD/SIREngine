// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_TERRAIN_MEGAVT_H
#define TERRAIN3D_TERRAIN_MEGAVT_H

#include "Terrain3DTypes.h"

#include "Terrain3DConstants.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "rendering/rd/vt/runtime/terrain_3d_virtual_texture.h"

#include <stdint.h>

#define T3D_TERRAIN_MEGAVT_MIN_TEXELS_PER_METER ( 2u )
#define T3D_TERRAIN_MEGAVT_MAX_TEXELS_PER_METER ( 4u )
#define T3D_TERRAIN_MEGAVT_DEFAULT_TEXELS_PER_METER ( 2u )
#define T3D_TERRAIN_MEGAVT_MAX_MIP0_PAGES ( 4u )
#define T3D_TERRAIN_MEGAVT_MAX_PAGES_PER_REGION ( 21u )
#define T3D_TERRAIN_MEGAVT_MAX_MIP0_PAGE_COUNT ( 16u )
#define T3D_TERRAIN_MEGAVT_ALL_MIP0_PAGES_MASK ( 0xffffu )

// Fixed CPU-side identity record. Allocation IDs are stable for a logical
// 32x32 world slot and survive region residency/unload transitions.

// P25 -> common P19 page-builder contract. P25 defines what terrain needs;
// the common cooker owns tiling, gutters, compression and package emission.

// 32-byte region draw record mirrored in terrain clipmap GLSL. This replaces
// the old scalar region-map entry while retaining its physical/shadow flags.

class Terrain3DTerrainMegaVT
{
public:
	Terrain3DTerrainMegaVT();

	bool attachVirtualTextureCold( const Terrain3DVirtualTexture *pVirtualTexture );
	void resetCold();
	bool syncRegionPresenceHot( const uint16_t *pLogicalRegionMap, uint64_t nTopologyGeneration );
	bool buildCookRequest( uint32_t nRegionMapIndex, t3dTerrainMegaVTCookRequest_t& rRequest ) const;
	bool setTexelsPerMeterCold( uint32_t nTexelsPerMeter );

	uint32_t requestForceRecookAllRegionsCold( void );
	bool markEditorDirtyBoundsHot( const t3dTerrainMegaVTEditBounds_t& rBounds );
	bool importPersistentEditorStateHot( uint32_t nRegionMapIndex, uint32_t nSourceGeneration, uint32_t nDirtyMip0Mask );
	bool getPersistentEditorState( uint32_t nRegionMapIndex, uint32_t& rSourceGeneration, uint32_t& rDirtyMip0Mask ) const;
	bool claimIncrementalCookHot( uint32_t nRegionMapIndex, t3dTerrainMegaVTIncrementalCookRequest_t& rRequest,
	                              t3dTerrainMegaVTCookTicket_t& rTicket );
	bool validateIncrementalCookCompletionHot( const t3dTerrainMegaVTCookTicket_t& rTicket );
	bool acceptIncrementalCookCompletionHot( const t3dTerrainMegaVTCookTicket_t& rTicket );
	// Owner-thread completion, failure and cancellation all retire the exact claim.
	bool cancelIncrementalCookHot( const t3dTerrainMegaVTCookTicket_t& rTicket );

	uint64_t getEditorDirtyGeneration() const;
	uint64_t getStaleCompletionRejectCount() const;
	uint32_t getDirtyRegionCount() const;

	const t3dTerrainMegaVTRegion_t *getRegion( uint32_t nRegionMapIndex ) const;
	uint64_t getPublicationGeneration() const;
	uint64_t getTopologyGeneration() const;
	uint32_t getTexelsPerMeter() const;
	bool isReady() const;

	static bool computeFootprint( uint32_t nTexelsPerMeter, uint16_t& rMip0WidthPages, uint16_t& rMip0HeightPages, uint16_t& rMipCount,
	                              uint32_t& rPageCount );
	static void regionIndexToLocation( uint32_t nRegionMapIndex, int32_t& rX, int32_t& rZ );
	static bool locationToRegionIndex( int32_t nRegionX, int32_t nRegionZ, uint32_t& rRegionMapIndex );
	static uint32_t computeDirtyPageClosure( uint16_t nMip0WidthPages, uint16_t nMip0HeightPages, uint16_t nMipCount,
	                                         uint32_t nDirtyMip0Mask );

private:
	static uint32_t makeContentId( int32_t nRegionX, int32_t nRegionZ, uint32_t nTexelsPerMeter );
	void initializeRegionIdentity( uint32_t nRegionMapIndex, t3dTerrainMegaVTRegion_t& rRegion,
	                               uint32_t nAllocationId = T3D_VT_INVALID_ALLOCATION );
	uint32_t computeRegionDirtyMip0Mask( const t3dTerrainMegaVTRegion_t& rRegion, int32_t nRegionX, int32_t nRegionZ,
	                                     const t3dTerrainMegaVTEditBounds_t& rBounds ) const;
	bool isIncrementalCookCompletionCurrent( const t3dTerrainMegaVTCookTicket_t& rTicket ) const;
	void advancePublicationGeneration();

	const Terrain3DVirtualTexture *m_pVirtualTexture;
	t3dTerrainMegaVTRegion_t m_szRegions[T3D_REGION_SLOTS];
	t3dTerrainMegaVTEditorState_t m_szEditorState[T3D_REGION_SLOTS];
	uint32_t m_szPackageAllocationSnapshot[T3D_REGION_SLOTS]{};
	uint32_t m_szPackageCookedGenerationSnapshot[T3D_REGION_SLOTS]{};
	uint8_t m_szPackageCookedValidSnapshot[T3D_REGION_SLOTS]{};
	uint64_t m_szInFlightClaimIds[T3D_REGION_SLOTS]{};
	uint64_t m_nNextCookClaimId = 1u; // Never reset across cold resets/reconfiguration.
	uint64_t m_nPublicationGeneration;
	uint64_t m_nTopologyGeneration;
	uint32_t m_nTexelsPerMeter;
	uint32_t m_nDirtyRegionCount;
	uint64_t m_nEditorDirtyGeneration;
	uint64_t m_nStaleCompletionRejectCount;
	bool m_bAttached;
};

static_assert( T3D_REGION_WORLD_SIZE_METERS == 128, "P25 MegaVT requires the fork's 128 m region specialization" );
static_assert( T3D_REGION_SLOTS == 1024, "P25 MegaVT stable allocation domain requires 1024 logical region slots" );
static_assert( sizeof( t3dTerrainMegaVTRegion_t ) == 48u, "Terrain MegaVT CPU region ABI changed" );
static_assert( sizeof( t3dTerrainMegaVTEditorState_t ) == 16u, "Terrain MegaVT editor state ABI changed" );
static_assert( sizeof( t3dTerrainMegaVTCookTicket_t ) == 32u, "Terrain MegaVT cook ticket ABI changed" );
static_assert( sizeof( t3dRdTerrainRegionGpu_t ) == 64u, "Terrain MegaVT draw-region ABI changed" );

#endif // TERRAIN3D_TERRAIN_MEGAVT_H

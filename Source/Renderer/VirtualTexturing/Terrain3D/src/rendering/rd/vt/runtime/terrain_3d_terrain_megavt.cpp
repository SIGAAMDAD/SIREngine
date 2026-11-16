// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/runtime/terrain_3d_terrain_megavt.h"
#include "core/utility/terrain_3d_generation.h"
#include "core/utility/terrain_3d_hash_bytes.h"

#include "rendering/rd/vt/runtime/terrain_3d_virtual_texture.h"

#include <math.h>
#include <string.h>

Terrain3DTerrainMegaVT::Terrain3DTerrainMegaVT()
{
	m_pVirtualTexture = NULL;
	memset( m_szRegions, 0, sizeof( m_szRegions ) );
	memset( m_szEditorState, 0, sizeof( m_szEditorState ) );
	for ( uint32_t i = 0u; i < ( uint32_t )( T3D_REGION_SLOTS ); ++i ) {
		m_szRegions[i].m_nAllocationId = T3D_VT_INVALID_ALLOCATION;
	}
	m_nPublicationGeneration = 0u;
	m_nTopologyGeneration = 0u;
	m_nTexelsPerMeter = T3D_TERRAIN_MEGAVT_DEFAULT_TEXELS_PER_METER;
	m_nDirtyRegionCount = 0u;
	m_nEditorDirtyGeneration = 0u;
	m_nStaleCompletionRejectCount = 0u;
	m_bAttached = false;
}

bool Terrain3DTerrainMegaVT::attachVirtualTextureCold( const Terrain3DVirtualTexture *const pVirtualTexture )
{
	if ( pVirtualTexture == NULL || ( m_pVirtualTexture != NULL && m_pVirtualTexture != pVirtualTexture ) ) {
		return false;
	}
	m_pVirtualTexture = pVirtualTexture;
	m_bAttached = true;
	return true;
}

void Terrain3DTerrainMegaVT::resetCold()
{
	memset( m_szInFlightClaimIds, 0, sizeof( m_szInFlightClaimIds ) );
	const Terrain3DVirtualTexture *const pOwner = m_pVirtualTexture;
	const uint32_t nTexelsPerMeter = m_nTexelsPerMeter;
	memset( m_szRegions, 0, sizeof( m_szRegions ) );
	memset( m_szEditorState, 0, sizeof( m_szEditorState ) );
	for ( uint32_t i = 0u; i < ( uint32_t )( T3D_REGION_SLOTS ); ++i ) {
		m_szRegions[i].m_nAllocationId = T3D_VT_INVALID_ALLOCATION;
	}
	m_pVirtualTexture = pOwner;
	m_nPublicationGeneration = 0u;
	m_nTopologyGeneration = 0u;
	m_nTexelsPerMeter = nTexelsPerMeter;
	m_nDirtyRegionCount = 0u;
	m_nEditorDirtyGeneration = 0u;
	m_nStaleCompletionRejectCount = 0u;
	m_bAttached = pOwner != NULL;
}

bool Terrain3DTerrainMegaVT::computeFootprint( const uint32_t nTexelsPerMeter, uint16_t& rMip0WidthPages, uint16_t& rMip0HeightPages,
                                               uint16_t& rMipCount, uint32_t& rPageCount )
{
	if ( nTexelsPerMeter != T3D_TERRAIN_MEGAVT_MIN_TEXELS_PER_METER && nTexelsPerMeter != T3D_TERRAIN_MEGAVT_MAX_TEXELS_PER_METER ) {
		return false;
	}
	const uint32_t nSideTexels = ( uint32_t )( T3D_REGION_WORLD_SIZE_METERS )*nTexelsPerMeter;
	if ( nSideTexels == 0u || nSideTexels % T3D_VT_PAGE_INTERIOR != 0u ) {
		return false;
	}
	uint32_t nSidePages = nSideTexels / T3D_VT_PAGE_INTERIOR;
	if ( nSidePages == 0u || nSidePages > T3D_TERRAIN_MEGAVT_MAX_MIP0_PAGES ) {
		return false;
	}
	rMip0WidthPages = ( uint16_t )( nSidePages );
	rMip0HeightPages = ( uint16_t )( nSidePages );
	rMipCount = 0u;
	rPageCount = 0u;
	for ( ;; ) {
		rPageCount += nSidePages * nSidePages;
		++rMipCount;
		if ( nSidePages == 1u ) {
			break;
		}
		nSidePages >>= 1u;
	}
	return rPageCount <= T3D_TERRAIN_MEGAVT_MAX_PAGES_PER_REGION;
}

void Terrain3DTerrainMegaVT::regionIndexToLocation( const uint32_t nRegionMapIndex, int32_t& rX, int32_t& rZ )
{
	rX = ( int32_t )( nRegionMapIndex & ( uint32_t )( T3D_REGION_MAP_MASK ) ) - ( int32_t )( T3D_REGION_MAP_HALF );
	rZ = ( int32_t )( nRegionMapIndex >> ( uint32_t )( T3D_REGION_MAP_SHIFT ) ) - ( int32_t )( T3D_REGION_MAP_HALF );
}

uint32_t Terrain3DTerrainMegaVT::makeContentId( const int32_t nRegionX, const int32_t nRegionZ, const uint32_t nTexelsPerMeter )
{
	uint32_t hash = 2166136261u;
	const uint32_t values[4] = { ( uint32_t )( nRegionX ), ( uint32_t )( nRegionZ ), nTexelsPerMeter, 0x503235u };
	hash = t3dHashFnv1a32( values, sizeof( values ), hash );
	return hash == 0u ? 1u : hash;
}

void Terrain3DTerrainMegaVT::initializeRegionIdentity( const uint32_t nRegionMapIndex, t3dTerrainMegaVTRegion_t& rRegion,
                                                       const uint32_t nAllocationId )
{
	uint16_t nMip0Width = 0u;
	uint16_t nMip0Height = 0u;
	uint16_t nMipCount = 0u;
	uint32_t nPageCount = 0u;
	if ( !computeFootprint( m_nTexelsPerMeter, nMip0Width, nMip0Height, nMipCount, nPageCount ) ) {
		return;
	}
	int32_t nRegionX = 0;
	int32_t nRegionZ = 0;
	regionIndexToLocation( nRegionMapIndex, nRegionX, nRegionZ );
	rRegion.m_nAllocationId =
	    nAllocationId != T3D_VT_INVALID_ALLOCATION
	        ? nAllocationId
	        : ( m_pVirtualTexture ? m_pVirtualTexture->getTerrainMegaVTAllocationId( nRegionMapIndex ) : T3D_VT_INVALID_ALLOCATION );
	rRegion.m_nGeneration = rRegion.m_nGeneration == 0u ? 1u : rRegion.m_nGeneration;
	rRegion.m_nContentId = makeContentId( nRegionX, nRegionZ, m_nTexelsPerMeter );
	rRegion.m_vWorldToUv[0] = 1.0f / ( float )( T3D_REGION_WORLD_SIZE_METERS );
	rRegion.m_vWorldToUv[1] = 1.0f / ( float )( T3D_REGION_WORLD_SIZE_METERS );
	rRegion.m_vWorldToUv[2] = -( float )( nRegionX );
	rRegion.m_vWorldToUv[3] = -( float )( nRegionZ );
	rRegion.m_nMip0WidthPages = nMip0Width;
	rRegion.m_nMip0HeightPages = nMip0Height;
	rRegion.m_nMipCount = nMipCount;
	rRegion.m_nTexelsPerMeter = ( uint16_t )( m_nTexelsPerMeter );
	rRegion.m_nPageCount = nPageCount;
}

bool Terrain3DTerrainMegaVT::syncRegionPresenceHot( const uint16_t *const pLogicalRegionMap, const uint64_t nTopologyGeneration )
{
	if ( !m_bAttached || m_pVirtualTexture == NULL || !m_pVirtualTexture->is_initialized() || pLogicalRegionMap == NULL ) {
		return false;
	}
	if ( !m_pVirtualTexture->snapshotTerrainMegaVTReadStateCold( m_szPackageAllocationSnapshot, m_szPackageCookedGenerationSnapshot,
	                                                             m_szPackageCookedValidSnapshot, T3D_REGION_SLOTS ) ) {
		return false;
	}
	bool bChanged = nTopologyGeneration != m_nTopologyGeneration;
	for ( uint32_t i = 0u; i < ( uint32_t )( T3D_REGION_SLOTS ); ++i ) {
		t3dTerrainMegaVTRegion_t& region = m_szRegions[i];
		if ( ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_COOKED ) != 0u ) {
			const uint32_t nAllocation = m_szPackageAllocationSnapshot[i];
			bChanged |= nAllocation != region.m_nAllocationId;
			region.m_nAllocationId = nAllocation;
		}
		const bool bPresent = pLogicalRegionMap[i] != 0u;
		const bool bWasPresent = ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) != 0u;
		if ( region.m_nAllocationId == T3D_VT_INVALID_ALLOCATION ) {
			initializeRegionIdentity( i, region, m_szPackageAllocationSnapshot[i] );
			bChanged = true;
		}
		if ( bPresent != bWasPresent ) {
			if ( bPresent ) {
				region.m_nFlags |= T3D_TERRAIN_MEGAVT_REGION_PRESENT;
				if ( m_szEditorState[i].m_nDirtyMip0Mask == 0u && m_szPackageCookedValidSnapshot[i] != 0u ) {
					region.m_nGeneration = m_szPackageCookedGenerationSnapshot[i];
					region.m_nFlags |= T3D_TERRAIN_MEGAVT_REGION_COOKED;
				}
			} else {
				region.m_nFlags &= ~T3D_TERRAIN_MEGAVT_REGION_PRESENT;
				if ( m_szEditorState[i].m_nDirtyMip0Mask != 0u && m_nDirtyRegionCount > 0u ) {
					--m_nDirtyRegionCount;
				}
				m_szEditorState[i] = t3dTerrainMegaVTEditorState_t{};
				m_szInFlightClaimIds[i] = 0u;
				region.m_nGeneration = t3dNextGeneration( region.m_nGeneration );
				region.m_nFlags &= ~( T3D_TERRAIN_MEGAVT_REGION_COOKED | T3D_TERRAIN_MEGAVT_REGION_EDITOR_DYNAMIC );
			}
			bChanged = true;
		}
	}
	m_nTopologyGeneration = nTopologyGeneration;
	if ( bChanged ) {
		advancePublicationGeneration();
	}
	return true;
}

bool Terrain3DTerrainMegaVT::buildCookRequest( const uint32_t nRegionMapIndex, t3dTerrainMegaVTCookRequest_t& rRequest ) const
{
	memset( &rRequest, 0, sizeof( rRequest ) );
	if ( nRegionMapIndex >= ( uint32_t )( T3D_REGION_SLOTS ) ) {
		return false;
	}
	const t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionMapIndex];
	if ( ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) == 0u || region.m_nAllocationId == T3D_VT_INVALID_ALLOCATION ) {
		return false;
	}
	int32_t nRegionX = 0;
	int32_t nRegionZ = 0;
	regionIndexToLocation( nRegionMapIndex, nRegionX, nRegionZ );
	rRequest.m_nAllocationId = region.m_nAllocationId;
	rRequest.m_nGeneration = region.m_nGeneration;
	rRequest.m_nContentId = region.m_nContentId;
	rRequest.m_nAllocationFlags =
	    T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP | T3D_VT_ALLOCATION_HAS_HEIGHT | T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
	rRequest.m_nRegionX = nRegionX;
	rRequest.m_nRegionZ = nRegionZ;
	rRequest.m_nMip0WidthPages = region.m_nMip0WidthPages;
	rRequest.m_nMip0HeightPages = region.m_nMip0HeightPages;
	rRequest.m_nMipCount = region.m_nMipCount;
	rRequest.m_nTexelsPerMeter = region.m_nTexelsPerMeter;
	rRequest.m_nPageCount = region.m_nPageCount;
	rRequest.m_nCookFlags = T3D_TERRAIN_MEGAVT_COOK_ALL_CHANNELS | T3D_TERRAIN_MEGAVT_COOK_NEIGHBOR_GUTTERS;
	rRequest.m_nPageInterior = T3D_VT_PAGE_INTERIOR;
	rRequest.m_nPageGutter = T3D_VT_PAGE_GUTTER;
	rRequest.m_nPhysicalTile = T3D_VT_PHYSICAL_TILE;
	memcpy( rRequest.m_vWorldToUv, region.m_vWorldToUv, sizeof( rRequest.m_vWorldToUv ) );
	return true;
}

void Terrain3DTerrainMegaVT::advancePublicationGeneration()
{
	m_nPublicationGeneration = t3dNextGeneration( m_nPublicationGeneration );
}

bool Terrain3DTerrainMegaVT::locationToRegionIndex( const int32_t nRegionX, const int32_t nRegionZ, uint32_t& rRegionMapIndex )
{
	if ( nRegionX < -( int32_t )( T3D_REGION_MAP_HALF ) || nRegionZ < -( int32_t )( T3D_REGION_MAP_HALF ) ||
	     nRegionX >= ( int32_t )( T3D_REGION_MAP_HALF ) || nRegionZ >= ( int32_t )( T3D_REGION_MAP_HALF ) ) {
		return false;
	}
	const uint32_t nMapX = ( uint32_t )( nRegionX + ( int32_t )( T3D_REGION_MAP_HALF ) );
	const uint32_t nMapZ = ( uint32_t )( nRegionZ + ( int32_t )( T3D_REGION_MAP_HALF ) );
	rRegionMapIndex = ( nMapZ << ( uint32_t )( T3D_REGION_MAP_SHIFT ) ) | nMapX;
	return rRegionMapIndex < ( uint32_t )( T3D_REGION_SLOTS );
}

uint32_t Terrain3DTerrainMegaVT::computeDirtyPageClosure( const uint16_t nMip0WidthPages, const uint16_t nMip0HeightPages,
                                                          const uint16_t nMipCount, const uint32_t nDirtyMip0Mask )
{
	if ( nMip0WidthPages == 0u || nMip0HeightPages == 0u || nMipCount == 0u || nMip0WidthPages > 4u || nMip0HeightPages > 4u ) {
		return 0u;
	}
	uint32_t nResult = 0u;
	uint32_t nCurrentMask = nDirtyMip0Mask & T3D_TERRAIN_MEGAVT_ALL_MIP0_PAGES_MASK;
	uint32_t nWidth = nMip0WidthPages;
	uint32_t nHeight = nMip0HeightPages;
	uint32_t nOffset = 0u;
	for ( uint32_t iMip = 0u; iMip < nMipCount && nWidth > 0u && nHeight > 0u; ++iMip ) {
		const uint32_t nMipPages = nWidth * nHeight;
		const uint32_t nMipMask = nMipPages >= 32u ? UINT32_MAX : ( ( 1u << nMipPages ) - 1u );
		nResult |= ( nCurrentMask & nMipMask ) << nOffset;
		if ( iMip + 1u >= nMipCount || ( nWidth == 1u && nHeight == 1u ) ) {
			break;
		}
		uint32_t nParentMask = 0u;
		const uint32_t nParentWidth = nWidth > 1u ? nWidth >> 1u : 1u;
		const uint32_t nParentHeight = nHeight > 1u ? nHeight >> 1u : 1u;
		for ( uint32_t iY = 0u; iY < nHeight; ++iY ) {
			for ( uint32_t iX = 0u; iX < nWidth; ++iX ) {
				const uint32_t nChildBit = iY * nWidth + iX;
				if ( ( nCurrentMask & ( 1u << nChildBit ) ) == 0u ) {
					continue;
				}
				const uint32_t nParentX = iX >> 1u;
				const uint32_t nParentY = iY >> 1u;
				nParentMask |= 1u << ( nParentY * nParentWidth + nParentX );
			}
		}
		nOffset += nMipPages;
		nWidth = nParentWidth;
		nHeight = nParentHeight;
		nCurrentMask = nParentMask;
	}
	return nResult;
}

uint32_t Terrain3DTerrainMegaVT::computeRegionDirtyMip0Mask( const t3dTerrainMegaVTRegion_t& rRegion, const int32_t nRegionX,
                                                             const int32_t nRegionZ, const t3dTerrainMegaVTEditBounds_t& rBounds ) const
{
	if ( rRegion.m_nMip0WidthPages == 0u || rRegion.m_nMip0HeightPages == 0u ) {
		return 0u;
	}
	const float fRegionSize = ( float )( T3D_REGION_WORLD_SIZE_METERS );
	const float fRegionMinX = ( float )( nRegionX )*fRegionSize;
	const float fRegionMinZ = ( float )( nRegionZ )*fRegionSize;
	const float fRegionMaxX = fRegionMinX + fRegionSize;
	const float fRegionMaxZ = fRegionMinZ + fRegionSize;
	const float fMinX = rBounds.m_fMinWorldX > fRegionMinX ? rBounds.m_fMinWorldX : fRegionMinX;
	const float fMinZ = rBounds.m_fMinWorldZ > fRegionMinZ ? rBounds.m_fMinWorldZ : fRegionMinZ;
	const float fMaxX = rBounds.m_fMaxWorldX < fRegionMaxX ? rBounds.m_fMaxWorldX : fRegionMaxX;
	const float fMaxZ = rBounds.m_fMaxWorldZ < fRegionMaxZ ? rBounds.m_fMaxWorldZ : fRegionMaxZ;
	if ( fMaxX <= fMinX || fMaxZ <= fMinZ ) {
		return 0u;
	}
	const float fPageWorldX = fRegionSize / ( float )( rRegion.m_nMip0WidthPages );
	const float fPageWorldZ = fRegionSize / ( float )( rRegion.m_nMip0HeightPages );
	int32_t nBeginX = ( int32_t )( floorf( ( fMinX - fRegionMinX ) / fPageWorldX ) );
	int32_t nBeginZ = ( int32_t )( floorf( ( fMinZ - fRegionMinZ ) / fPageWorldZ ) );
	int32_t nEndX = ( int32_t )( ceilf( ( fMaxX - fRegionMinX ) / fPageWorldX ) ) - 1;
	int32_t nEndZ = ( int32_t )( ceilf( ( fMaxZ - fRegionMinZ ) / fPageWorldZ ) ) - 1;
	nBeginX = nBeginX < 0 ? 0 : ( nBeginX >= rRegion.m_nMip0WidthPages ? rRegion.m_nMip0WidthPages - 1 : nBeginX );
	nBeginZ = nBeginZ < 0 ? 0 : ( nBeginZ >= rRegion.m_nMip0HeightPages ? rRegion.m_nMip0HeightPages - 1 : nBeginZ );
	nEndX = nEndX < 0 ? 0 : ( nEndX >= rRegion.m_nMip0WidthPages ? rRegion.m_nMip0WidthPages - 1 : nEndX );
	nEndZ = nEndZ < 0 ? 0 : ( nEndZ >= rRegion.m_nMip0HeightPages ? rRegion.m_nMip0HeightPages - 1 : nEndZ );
	uint32_t nMask = 0u;
	for ( int32_t iY = nBeginZ; iY <= nEndZ; ++iY ) {
		for ( int32_t iX = nBeginX; iX <= nEndX; ++iX ) {
			nMask |= 1u << ( ( uint32_t )( iY )*rRegion.m_nMip0WidthPages + ( uint32_t )( iX ) );
		}
	}
	return nMask;
}

bool Terrain3DTerrainMegaVT::markEditorDirtyBoundsHot( const t3dTerrainMegaVTEditBounds_t& rBounds )
{
	if ( !isReady() || rBounds.m_fMaxWorldX <= rBounds.m_fMinWorldX || rBounds.m_fMaxWorldZ <= rBounds.m_fMinWorldZ ) {
		return false;
	}
	const float fGutterWorld = ( float )( T3D_VT_PAGE_GUTTER ) / ( float )( m_nTexelsPerMeter );
	t3dTerrainMegaVTEditBounds_t expanded = rBounds;
	expanded.m_fMinWorldX -= fGutterWorld;
	expanded.m_fMinWorldZ -= fGutterWorld;
	expanded.m_fMaxWorldX += fGutterWorld;
	expanded.m_fMaxWorldZ += fGutterWorld;
	const float fRegionSize = ( float )( T3D_REGION_WORLD_SIZE_METERS );
	int32_t nMinRegionX = ( int32_t )( floorf( expanded.m_fMinWorldX / fRegionSize ) );
	int32_t nMinRegionZ = ( int32_t )( floorf( expanded.m_fMinWorldZ / fRegionSize ) );
	int32_t nMaxRegionX = ( int32_t )( floorf( ( expanded.m_fMaxWorldX - 0.0001f ) / fRegionSize ) );
	int32_t nMaxRegionZ = ( int32_t )( floorf( ( expanded.m_fMaxWorldZ - 0.0001f ) / fRegionSize ) );
	nMinRegionX = nMinRegionX < -( int32_t )( T3D_REGION_MAP_HALF ) ? -( int32_t )( T3D_REGION_MAP_HALF ) : nMinRegionX;
	nMinRegionZ = nMinRegionZ < -( int32_t )( T3D_REGION_MAP_HALF ) ? -( int32_t )( T3D_REGION_MAP_HALF ) : nMinRegionZ;
	nMaxRegionX = nMaxRegionX >= ( int32_t )( T3D_REGION_MAP_HALF ) ? ( int32_t )( T3D_REGION_MAP_HALF )-1 : nMaxRegionX;
	nMaxRegionZ = nMaxRegionZ >= ( int32_t )( T3D_REGION_MAP_HALF ) ? ( int32_t )( T3D_REGION_MAP_HALF )-1 : nMaxRegionZ;
	bool bChanged = false;
	for ( int32_t iRegionZ = nMinRegionZ; iRegionZ <= nMaxRegionZ; ++iRegionZ ) {
		for ( int32_t iRegionX = nMinRegionX; iRegionX <= nMaxRegionX; ++iRegionX ) {
			uint32_t nRegionIndex = 0u;
			if ( !locationToRegionIndex( iRegionX, iRegionZ, nRegionIndex ) ) {
				continue;
			}
			t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionIndex];
			if ( ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) == 0u ) {
				continue;
			}
			const uint32_t nMip0Mask = computeRegionDirtyMip0Mask( region, iRegionX, iRegionZ, expanded );
			if ( nMip0Mask == 0u ) {
				continue;
			}
			t3dTerrainMegaVTEditorState_t& editor = m_szEditorState[nRegionIndex];
			const bool bWasDirty = editor.m_nDirtyMip0Mask != 0u;
			editor.m_nDirtyMip0Mask |= nMip0Mask;
			editor.m_nDirtyPageMask =
			    computeDirtyPageClosure( region.m_nMip0WidthPages, region.m_nMip0HeightPages, region.m_nMipCount, editor.m_nDirtyMip0Mask );
			region.m_nGeneration = t3dNextGeneration( region.m_nGeneration );
			region.m_nFlags |= T3D_TERRAIN_MEGAVT_REGION_EDITOR_DYNAMIC;
			region.m_nFlags &= ~T3D_TERRAIN_MEGAVT_REGION_COOKED;
			if ( !bWasDirty ) {
				++m_nDirtyRegionCount;
			}
			bChanged = true;
		}
	}
	if ( bChanged ) {
		m_nEditorDirtyGeneration = t3dNextGeneration( m_nEditorDirtyGeneration );
		advancePublicationGeneration();
	}
	return bChanged;
}

bool Terrain3DTerrainMegaVT::importPersistentEditorStateHot( const uint32_t nRegionMapIndex, const uint32_t nSourceGeneration,
                                                             const uint32_t nDirtyMip0Mask )
{
	if ( nRegionMapIndex >= ( uint32_t )( T3D_REGION_SLOTS ) ) {
		return false;
	}
	t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionMapIndex];
	if ( region.m_nAllocationId == T3D_VT_INVALID_ALLOCATION ) {
		return false;
	}
	t3dTerrainMegaVTEditorState_t& editor = m_szEditorState[nRegionMapIndex];
	const uint32_t nMip0Pages = ( uint32_t )( region.m_nMip0WidthPages ) * region.m_nMip0HeightPages;
	const uint32_t nValidMip0Mask = nMip0Pages >= 32u ? UINT32_MAX : ( ( 1u << nMip0Pages ) - 1u );
	const uint32_t nSanitizedMask = nDirtyMip0Mask & nValidMip0Mask;
	const uint32_t nSanitizedGeneration = nSourceGeneration == 0u ? 1u : nSourceGeneration;
	if ( region.m_nGeneration == nSanitizedGeneration && editor.m_nDirtyMip0Mask == nSanitizedMask ) {
		return false;
	}
	const bool bWasDirty = editor.m_nDirtyMip0Mask != 0u;
	region.m_nGeneration = nSanitizedGeneration;
	editor.m_nDirtyMip0Mask = nSanitizedMask;
	editor.m_nDirtyPageMask =
	    computeDirtyPageClosure( region.m_nMip0WidthPages, region.m_nMip0HeightPages, region.m_nMipCount, editor.m_nDirtyMip0Mask );
	editor.m_nInFlightGeneration = 0u;
	m_szInFlightClaimIds[nRegionMapIndex] = 0u;
	if ( nSanitizedMask != 0u ) {
		region.m_nFlags |= T3D_TERRAIN_MEGAVT_REGION_EDITOR_DYNAMIC;
		region.m_nFlags &= ~T3D_TERRAIN_MEGAVT_REGION_COOKED;
	} else {
		region.m_nFlags &= ~T3D_TERRAIN_MEGAVT_REGION_EDITOR_DYNAMIC;
	}
	const bool bIsDirty = editor.m_nDirtyMip0Mask != 0u;
	if ( bWasDirty != bIsDirty ) {
		if ( bIsDirty ) {
			++m_nDirtyRegionCount;
		} else if ( m_nDirtyRegionCount > 0u ) {
			--m_nDirtyRegionCount;
		}
	}
	advancePublicationGeneration();
	return true;
}

bool Terrain3DTerrainMegaVT::getPersistentEditorState( const uint32_t nRegionMapIndex, uint32_t& rSourceGeneration,
                                                       uint32_t& rDirtyMip0Mask ) const
{
	if ( nRegionMapIndex >= ( uint32_t )( T3D_REGION_SLOTS ) ) {
		return false;
	}
	rSourceGeneration = m_szRegions[nRegionMapIndex].m_nGeneration == 0u ? 1u : m_szRegions[nRegionMapIndex].m_nGeneration;
	rDirtyMip0Mask = m_szEditorState[nRegionMapIndex].m_nDirtyMip0Mask;
	return true;
}

bool Terrain3DTerrainMegaVT::claimIncrementalCookHot( const uint32_t nRegionMapIndex, t3dTerrainMegaVTIncrementalCookRequest_t& rRequest,
                                                      t3dTerrainMegaVTCookTicket_t& rTicket )
{
	memset( &rRequest, 0, sizeof( rRequest ) );
	memset( &rTicket, 0, sizeof( rTicket ) );
	if ( !isReady() || nRegionMapIndex >= ( uint32_t )( T3D_REGION_SLOTS ) || m_nNextCookClaimId == 0u ) {
		return false;
	}
	t3dTerrainMegaVTEditorState_t& editor = m_szEditorState[nRegionMapIndex];
	if ( m_szInFlightClaimIds[nRegionMapIndex] != 0u || editor.m_nDirtyPageMask == 0u ||
	     !buildCookRequest( nRegionMapIndex, rRequest.m_base ) ) {
		return false;
	}
	t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionMapIndex];
	rRequest.m_base.m_nAllocationFlags &= ~T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE;
	rRequest.m_base.m_nAllocationFlags |= T3D_VT_ALLOCATION_EDITOR_DYNAMIC;
	rRequest.m_nDirtyMip0Mask = editor.m_nDirtyMip0Mask;
	rRequest.m_nDirtyPageMask = editor.m_nDirtyPageMask;
	rRequest.m_nSourceGeneration = region.m_nGeneration;
	rRequest.m_nIncrementalFlags =
	    T3D_TERRAIN_MEGAVT_INCREMENTAL_EDITOR | T3D_TERRAIN_MEGAVT_INCREMENTAL_ANCESTORS | T3D_TERRAIN_MEGAVT_INCREMENTAL_GUTTER_NEIGHBORS;
	rTicket.m_nAllocationId = region.m_nAllocationId;
	rTicket.m_nRegionMapIndex = nRegionMapIndex;
	rTicket.m_nGeneration = region.m_nGeneration;
	rTicket.m_nContentId = region.m_nContentId;
	rTicket.m_nPageMask = editor.m_nDirtyPageMask;
	rTicket.m_nClaimId = m_nNextCookClaimId++;
	m_szInFlightClaimIds[nRegionMapIndex] = rTicket.m_nClaimId;
	editor.m_nInFlightGeneration = region.m_nGeneration;
	return true;
}

bool Terrain3DTerrainMegaVT::isIncrementalCookCompletionCurrent( const t3dTerrainMegaVTCookTicket_t& rTicket ) const
{
	if ( !isReady() || rTicket.m_nClaimId == 0u || rTicket.m_nAllocationId == T3D_VT_INVALID_ALLOCATION ) {
		return false;
	}
	const uint32_t nRegionMapIndex = rTicket.m_nRegionMapIndex;
	if ( nRegionMapIndex >= ( uint32_t )( T3D_REGION_SLOTS ) ) {
		return false;
	}
	const t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionMapIndex];
	const t3dTerrainMegaVTEditorState_t& editor = m_szEditorState[nRegionMapIndex];
	return ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) != 0u && m_szInFlightClaimIds[nRegionMapIndex] == rTicket.m_nClaimId &&
	       rTicket.m_nGeneration == region.m_nGeneration && rTicket.m_nContentId == region.m_nContentId &&
	       rTicket.m_nPageMask == editor.m_nDirtyPageMask && editor.m_nInFlightGeneration == rTicket.m_nGeneration;
}

bool Terrain3DTerrainMegaVT::validateIncrementalCookCompletionHot( const t3dTerrainMegaVTCookTicket_t& rTicket )
{
	// P19/WG-D must call this after asynchronous cook completion and immediately
	// before publishing the transaction into the common VT owner. Publication
	// itself must carry the same allocation generation so the owner can perform
	// its own transactional generation check.
	if ( isIncrementalCookCompletionCurrent( rTicket ) ) {
		return true;
	}
	++m_nStaleCompletionRejectCount;
	cancelIncrementalCookHot( rTicket );
	return false;
}

bool Terrain3DTerrainMegaVT::acceptIncrementalCookCompletionHot( const t3dTerrainMegaVTCookTicket_t& rTicket )
{
	// Acceptance occurs only after the common owner has published every page in
	// this ticket. Recheck here because an editor stroke may have advanced the
	// generation between worker validation and publication completion.
	if ( !isIncrementalCookCompletionCurrent( rTicket ) ) {
		++m_nStaleCompletionRejectCount;
		cancelIncrementalCookHot( rTicket );
		return false;
	}
	const uint32_t nRegionMapIndex = rTicket.m_nRegionMapIndex;
	t3dTerrainMegaVTRegion_t& region = m_szRegions[nRegionMapIndex];
	t3dTerrainMegaVTEditorState_t& editor = m_szEditorState[nRegionMapIndex];
	editor.m_nDirtyMip0Mask = 0u;
	editor.m_nDirtyPageMask = 0u;
	editor.m_nInFlightGeneration = 0u;
	editor.m_nLastPublishedGeneration = region.m_nGeneration;
	m_szInFlightClaimIds[nRegionMapIndex] = 0u;
	region.m_nAllocationId = m_pVirtualTexture->getTerrainMegaVTAllocationId( nRegionMapIndex );
	region.m_nFlags |= T3D_TERRAIN_MEGAVT_REGION_COOKED;
	region.m_nFlags &= ~T3D_TERRAIN_MEGAVT_REGION_EDITOR_DYNAMIC;
	if ( m_nDirtyRegionCount > 0u ) {
		--m_nDirtyRegionCount;
	}
	advancePublicationGeneration();
	return true;
}

bool Terrain3DTerrainMegaVT::cancelIncrementalCookHot( const t3dTerrainMegaVTCookTicket_t& rTicket )
{
	if ( rTicket.m_nClaimId == 0u ) {
		return false;
	}
	const uint32_t index = rTicket.m_nRegionMapIndex;
	if ( index >= ( uint32_t )T3D_REGION_SLOTS || m_szInFlightClaimIds[index] != rTicket.m_nClaimId ) {
		return false;
	}
	m_szInFlightClaimIds[index] = 0u;
	m_szEditorState[index].m_nInFlightGeneration = 0u;
	// Dirty masks survive failures/cancellation so the next claim can retry.
	return true;
}

bool Terrain3DTerrainMegaVT::setTexelsPerMeterCold( const uint32_t nTexelsPerMeter )
{
	uint16_t nWidth = 0u;
	uint16_t nHeight = 0u;
	uint16_t nMips = 0u;
	uint32_t nPages = 0u;
	if ( !computeFootprint( nTexelsPerMeter, nWidth, nHeight, nMips, nPages ) ) {
		return false;
	}
	if ( m_nTexelsPerMeter == nTexelsPerMeter ) {
		return true;
	}
	m_nTexelsPerMeter = nTexelsPerMeter;
	memset( m_szInFlightClaimIds, 0, sizeof( m_szInFlightClaimIds ) );
	m_nDirtyRegionCount = 0u;
	memset( m_szEditorState, 0, sizeof( m_szEditorState ) );
	for ( uint32_t i = 0u; i < ( uint32_t )( T3D_REGION_SLOTS ); ++i ) {
		const uint32_t nFlags = m_szRegions[i].m_nFlags;
		const uint32_t nGeneration = m_szRegions[i].m_nGeneration == 0u ? 1u : m_szRegions[i].m_nGeneration + 1u;
		memset( &m_szRegions[i], 0, sizeof( m_szRegions[i] ) );
		m_szRegions[i].m_nAllocationId = T3D_VT_INVALID_ALLOCATION;
		m_szRegions[i].m_nGeneration = nGeneration;
		m_szRegions[i].m_nFlags = nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT;
		if ( m_pVirtualTexture && m_pVirtualTexture->is_initialized() ) {
			initializeRegionIdentity( i, m_szRegions[i] );
		}
	}
	advancePublicationGeneration();
	return true;
}

const t3dTerrainMegaVTRegion_t *Terrain3DTerrainMegaVT::getRegion( const uint32_t nRegionMapIndex ) const
{
	return nRegionMapIndex < ( uint32_t )( T3D_REGION_SLOTS ) ? &m_szRegions[nRegionMapIndex] : NULL;
}

uint64_t Terrain3DTerrainMegaVT::getPublicationGeneration() const
{
	return m_nPublicationGeneration;
}
uint64_t Terrain3DTerrainMegaVT::getTopologyGeneration() const
{
	return m_nTopologyGeneration;
}
uint32_t Terrain3DTerrainMegaVT::getTexelsPerMeter() const
{
	return m_nTexelsPerMeter;
}
uint64_t Terrain3DTerrainMegaVT::getEditorDirtyGeneration() const
{
	return m_nEditorDirtyGeneration;
}
uint64_t Terrain3DTerrainMegaVT::getStaleCompletionRejectCount() const
{
	return m_nStaleCompletionRejectCount;
}
uint32_t Terrain3DTerrainMegaVT::getDirtyRegionCount() const
{
	return m_nDirtyRegionCount;
}
bool Terrain3DTerrainMegaVT::isReady() const
{
	return m_bAttached && m_pVirtualTexture != NULL && m_pVirtualTexture->is_initialized();
}

uint32_t Terrain3DTerrainMegaVT::requestForceRecookAllRegionsCold( void )
{
	uint32_t count = 0u;
	for ( uint32_t index = 0u; index < T3D_REGION_SLOTS; ++index ) {
		const t3dTerrainMegaVTRegion_t& region = m_szRegions[index];
		if ( ( region.m_nFlags & T3D_TERRAIN_MEGAVT_REGION_PRESENT ) == 0u ) {
			continue;
		}
		const uint32_t pages = region.m_nMip0WidthPages * region.m_nMip0HeightPages;
		const uint32_t mask = pages >= 32u ? UINT32_MAX : ( 1u << pages ) - 1u;
		// A forced derived-data rebuild is not an authored source edit.
		importPersistentEditorStateHot( index, region.m_nGeneration, mask );
		++count;
	}
	return count;
}

// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/planning/terrain_3d_vt_resource_plan.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "core/utility/terrain_3d_checked_math.h"

#include <limits.h>
#include <string.h>

static bool t3d_vt_add_plan_bytes( uint64_t pBytes, uint64_t& rAccumulator )
{
	uint64_t nNext = 0u;
	if ( !t3dCheckedAdd<uint64_t>( rAccumulator, pBytes, nNext ) ) {
		return false;
	}

	rAccumulator = nNext;
	return true;
}

bool t3d_vt_fit_resource_plan_to_budget( t3dWorldLimits_t& rLimits, const t3dVTGpuProfile_t& rProfile, uint64_t budget_bytes,
                                         t3dVTGpuResourcePlan_t& rPlan, uint32_t active_tail_height )
{
	t3dWorldLimits_t candidate = rLimits;
	t3dVTGpuResourcePlan_t plan{};
	if ( candidate.nMaxVtPhysicalPageSets < T3D_VT_MIN_PHYSICAL_PAGE_SETS ) {
		return false;
	}
	if ( !t3d_vt_compute_resource_plan( candidate, rProfile, plan, active_tail_height ) ) {
		return false;
	}
	if ( plan.nTotalBytes <= budget_bytes ) {
		rPlan = plan;
		return true;
	}
	candidate.nMaxVtPhysicalPageSets = T3D_VT_MIN_PHYSICAL_PAGE_SETS;
	if ( !t3d_vt_compute_resource_plan( candidate, rProfile, plan, active_tail_height ) || plan.nTotalBytes > budget_bytes ) {
		return false;
	}
	uint32_t lower = T3D_VT_MIN_PHYSICAL_PAGE_SETS, upper = rLimits.nMaxVtPhysicalPageSets;
	while ( lower < upper ) {
		const uint32_t middle = lower + ( upper - lower + 1u ) / 2u;
		candidate.nMaxVtPhysicalPageSets = middle;
		if ( !t3d_vt_compute_resource_plan( candidate, rProfile, plan, active_tail_height ) ) {
			return false;
		}
		if ( plan.nTotalBytes <= budget_bytes ) {
			lower = middle;
		} else {
			upper = middle - 1u;
		}
	}
	candidate.nMaxVtPhysicalPageSets = lower;
	if ( !t3d_vt_compute_resource_plan( candidate, rProfile, plan, active_tail_height ) ) {
		return false;
	}
	rLimits = candidate;
	rPlan = plan;
	return true;
}

static bool t3d_vt_compute_block_texture_bytes( uint32_t pWidth, uint32_t pHeight, uint32_t pLayers, uint32_t pBlockWidth,
                                                uint32_t pBlockHeight, uint32_t pBlockBytes, uint64_t& rBytes )
{
	if ( pWidth == 0u || pHeight == 0u || pLayers == 0u || pBlockWidth == 0u || pBlockHeight == 0u || pBlockBytes == 0u ) {
		return false;
	}

	const uint64_t nBlocksX = ( ( uint64_t )pWidth + pBlockWidth - 1u ) / pBlockWidth;
	const uint64_t nBlocksY = ( ( uint64_t )pHeight + pBlockHeight - 1u ) / pBlockHeight;
	uint64_t nBlocks = 0u;
	uint64_t nLayerBytes = 0u;
	if ( !t3dCheckedMultiply<uint64_t>( nBlocksX, nBlocksY, nBlocks ) ) {
		return false;
	}
	if ( !t3dCheckedMultiply<uint64_t>( nBlocks, pBlockBytes, nLayerBytes ) ) {
		return false;
	}
	return t3dCheckedMultiply<uint64_t>( nLayerBytes, pLayers, rBytes );
}

t3dVTGpuProfile_t t3d_vt_make_development_gpu_profile()
{
	t3dVTGpuProfile_t profile = {};
	profile.nVtAllocationRecordBytes = sizeof( t3dVTAllocation_t );
	profile.nVtPageTableEntryBytes = sizeof( t3dVTPageEntry_t );
	profile.nVtPhysicalTileSize = 136u;
	return profile;
}

t3dVTGpuProfile_t t3d_vt_make_runtime_gpu_profile( const t3dWorldResourceLimits_t& )
{
	return t3d_vt_make_development_gpu_profile();
}

t3dVTGpuProfile_t t3d_vt_make_preview_gpu_profile()
{
	return t3d_vt_make_development_gpu_profile();
}

bool t3d_vt_validate_gpu_profile( const t3dVTGpuProfile_t& pProfile )
{
	return pProfile.nVtAllocationRecordBytes == sizeof( t3dVTAllocation_t ) &&
	       pProfile.nVtPageTableEntryBytes == sizeof( t3dVTPageEntry_t ) && pProfile.nVtPhysicalTileSize != 0u;
}

bool t3d_vt_compute_bank_layout( uint32_t pPageCount, uint32_t pMaxArrayLayers, t3dVTBankLayout_t& rLayout )
{
	memset( &rLayout, 0, sizeof( rLayout ) );
	if ( pPageCount == 0u || pMaxArrayLayers == 0u || pPageCount > pMaxArrayLayers ) {
		return false;
	}
	rLayout.nBankCount = 1u;
	rLayout.nLayers[0] = pPageCount;
	return true;
}

bool t3d_vt_compute_active_tail_height_cold( const t3dVTTailMetadata_t *const pTails, const uint32_t nTailCount, uint32_t& rHeight )
{
	rHeight = 0u;
	if ( pTails == NULL || nTailCount == 0u )
		return false;
	for ( uint32_t i = 0u; i < nTailCount; ++i ) {
		const uint32_t nY = pTails[i].m_nAtlasY;
		if ( nY % T3D_VT_PACK_TAIL_SLOT_HEIGHT != 0u || nY > UINT32_MAX - T3D_VT_PACK_TAIL_SLOT_HEIGHT )
			return false;
		const uint32_t nEnd = nY + T3D_VT_PACK_TAIL_SLOT_HEIGHT;
		if ( nEnd > rHeight )
			rHeight = nEnd;
	}
	return true;
}

bool t3d_vt_compute_resource_plan( const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile, t3dVTGpuResourcePlan_t& rPlan,
                                   const uint32_t nActiveTailAtlasHeight )
{
	memset( &rPlan, 0, sizeof( rPlan ) );
	if ( !t3d_vt_validate_gpu_profile( pProfile ) || pLimits.nMaxMaterials == 0u || pLimits.nMaxVtVirtualPages == 0u ||
	     pLimits.nMaxVtPhysicalPageSets == 0u )
		return false;
	uint64_t nBytes = 0u;
	uint64_t nCount = 0u;
	if ( !t3dCheckedMultiply<uint64_t>( pLimits.nMaxMaterials, 4u, nCount ) ||
	     !t3dCheckedAdd<uint64_t>( nCount, ( uint64_t )T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 12u, nCount ) ||
	     !t3dCheckedMultiply<uint64_t>( nCount, pProfile.nVtAllocationRecordBytes, nBytes ) )
		return false;
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ALLOCATION_TABLE] = nBytes;
	uint64_t nMipLayoutEntries = 0u;
	if ( !t3dCheckedMultiply<uint64_t>( nCount, T3D_VT_MAX_MIPS, nMipLayoutEntries ) ||
	     !t3dCheckedMultiply<uint64_t>( nMipLayoutEntries, sizeof( t3dVTMipLayout_t ), nBytes ) )
		return false;
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_MIP_LAYOUT_TABLE] = nBytes;
	if ( !t3dCheckedMultiply<uint64_t>( pLimits.nMaxVtVirtualPages, pProfile.nVtPageTableEntryBytes, nBytes ) ) {
		return false;
	}
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_PAGE_TABLE] = nBytes;
#if defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) && T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
	// One 16-byte control record and one record per virtual page. Cold opt-in.
	if ( !t3dCheckedMultiply<uint64_t>( ( uint64_t )pLimits.nMaxVtVirtualPages + 1u, 16u, nBytes ) || nBytes > UINT32_MAX )
		return false;
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_ANCESTOR_HINTS] = nBytes;
#endif

	uint64_t nTailEntries = 0u;
	if ( !t3dCheckedAdd<uint64_t>( pLimits.nMaxMaterials, T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u, nTailEntries ) ||
	     !t3dCheckedMultiply<uint64_t>( nTailEntries, 4u, nTailEntries ) || nTailEntries == 0u || nTailEntries > UINT32_MAX ) {
		return false;
	}
	if ( !t3dCheckedMultiply<uint64_t>( nTailEntries, sizeof( t3dVTTailMetadata_t ), nBytes ) ) {
		return false;
	}
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_TAIL_METADATA] = nBytes;
	rPlan.nVtTailEntryCapacity = ( uint32_t )nTailEntries;
	const uint32_t feedback_entries =
	    MAX( 2u, MIN( pLimits.nMaxVtRequests, ( pLimits.nMaxReadbackBytesPerFrame > sizeof( t3dVTRequestGpuCounters_t )
	                                                ? pLimits.nMaxReadbackBytesPerFrame - ( uint32_t )sizeof( t3dVTRequestGpuCounters_t )
	                                                : 0u ) /
		                                          ( uint32_t )sizeof( t3dVTRequestHashEntry_t ) ) );
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_FRAGMENT_HASH] = ( uint64_t )feedback_entries * sizeof( t3dVTRequestHashEntry_t );
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_FRAGMENT_COUNTERS] = sizeof( t3dVTRequestGpuCounters_t );
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_FRAGMENT_PARAMS] = sizeof( t3dVTFragmentFeedbackParams_t );
	t3dVTFeedbackGridLayout_t feedbackLayout{};
	if ( !t3d_vt_feedback_grid_layout( pLimits.nMaxReadbackBytesPerFrame, feedbackLayout ) )
		return false;
	rPlan.nBufferBytes[T3D_VT_GPU_BUFFER_FEEDBACK_GRID] = feedbackLayout.m_nTotalBytes;
	rPlan.nVtTailAtlasWidth =
	    T3D_VT_PACK_TAIL_ATLAS_WIDTH > T3D_VT_TAIL_ATLAS_WIDTH ? T3D_VT_PACK_TAIL_ATLAS_WIDTH : T3D_VT_TAIL_ATLAS_WIDTH;
	const uint64_t nLegacyTailRows64 = ( nTailEntries + T3D_VT_TAIL_TILES_PER_ROW - 1u ) / T3D_VT_TAIL_TILES_PER_ROW;
	// BC1 can hold two logical channels (instance ALBEDO and ORM).
	const uint64_t nPackFamilySlots = ( ( uint64_t )pLimits.nMaxMaterials + T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u ) * 3u;
	const uint64_t nPackTailRows64 = ( nPackFamilySlots + T3D_VT_PACK_TAILS_PER_ROW - 1u ) / T3D_VT_PACK_TAILS_PER_ROW;
	if ( T3D_VT_PACK_TAILS_PER_ROW == 0u || nLegacyTailRows64 == 0u || nPackTailRows64 == 0u ||
	     nLegacyTailRows64 > UINT32_MAX / T3D_VT_TAIL_TILE || nPackTailRows64 > UINT32_MAX / T3D_VT_PACK_TAIL_SLOT_HEIGHT ) {
		return false;
	}
	const uint32_t nLegacyTailHeight = ( uint32_t )nLegacyTailRows64 * T3D_VT_TAIL_TILE;
	const uint32_t nPackTailHeight = ( uint32_t )nPackTailRows64 * T3D_VT_PACK_TAIL_SLOT_HEIGHT;
	rPlan.nVtTailAtlasHeight = nLegacyTailHeight > nPackTailHeight ? nLegacyTailHeight : nPackTailHeight;
	// Table capacities stay fixed; texture storage only needs the currently
	// published tails. Package replacement already recreates this resource image.
	if ( nActiveTailAtlasHeight != 0u ) {
		if ( nActiveTailAtlasHeight % 4u != 0u || nActiveTailAtlasHeight > rPlan.nVtTailAtlasHeight )
			return false;
		rPlan.nVtTailAtlasHeight = nActiveTailAtlasHeight;
	}

	for ( uint32_t iBuffer = 0u; iBuffer < T3D_VT_GPU_BUFFER_COUNT; ++iBuffer ) {
		if ( rPlan.nBufferBytes[iBuffer] == 0u ) {
#if !defined( T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS ) || !T3D_ENABLE_VT_RESIDENT_ANCESTOR_HINTS
			// The ancestor-hint buffer is optional. GPU storage already skips zero-sized entries.
			if ( iBuffer == T3D_VT_GPU_BUFFER_ANCESTOR_HINTS )
				continue;
#endif
			return false;
		}
		if ( !t3d_vt_add_plan_bytes( rPlan.nBufferBytes[iBuffer], rPlan.nTotalBufferBytes ) ) {
			return false;
		}
	}

	const uint32_t nTile = pProfile.nVtPhysicalTileSize;
	if ( !t3d_vt_compute_block_texture_bytes( nTile, nTile, pLimits.nMaxVtPhysicalPageSets, 4u, 4u, 8u,
	                                          rPlan.nVtPhysicalBytes[T3D_VT_RESOURCE_ALBEDO] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTile, nTile, pLimits.nMaxVtPhysicalPageSets, 4u, 4u, 16u,
	                                          rPlan.nVtPhysicalBytes[T3D_VT_RESOURCE_NORMAL] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTile, nTile, pLimits.nMaxVtPhysicalPageSets, 4u, 4u, 16u,
	                                          rPlan.nVtPhysicalBytes[T3D_VT_RESOURCE_SURFACE] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTile, nTile, pLimits.nMaxVtPhysicalPageSets, 1u, 1u, 1u,
	                                          rPlan.nVtPhysicalBytes[T3D_VT_RESOURCE_HEIGHT] ) ) {
		return false;
	}

	if ( !t3d_vt_compute_block_texture_bytes( nTile, nTile, pLimits.nMaxVtPhysicalPageSets, 1u, 1u, 2u,
	                                          rPlan.nVtPhysicalBytes[T3D_VT_RESOURCE_HEIGHT16] ) ) {
		return false;
	}

	const uint32_t nTailWidth = rPlan.nVtTailAtlasWidth;
	const uint32_t nTailHeight = rPlan.nVtTailAtlasHeight;
	if ( !t3d_vt_compute_block_texture_bytes( nTailWidth, nTailHeight, 1u, 4u, 4u, 8u, rPlan.nVtTailBytes[T3D_VT_RESOURCE_ALBEDO] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTailWidth, nTailHeight, 1u, 4u, 4u, 16u, rPlan.nVtTailBytes[T3D_VT_RESOURCE_NORMAL] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTailWidth, nTailHeight, 1u, 4u, 4u, 16u, rPlan.nVtTailBytes[T3D_VT_RESOURCE_SURFACE] ) ) {
		return false;
	}
	if ( !t3d_vt_compute_block_texture_bytes( nTailWidth, nTailHeight, 1u, 1u, 1u, 1u, rPlan.nVtTailBytes[T3D_VT_RESOURCE_HEIGHT] ) ) {
		return false;
	}

	if ( !t3d_vt_compute_block_texture_bytes( nTailWidth, nTailHeight, 1u, 1u, 1u, 2u, rPlan.nVtTailBytes[T3D_VT_RESOURCE_HEIGHT16] ) ) {
		return false;
	}

	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_CHANNEL_COUNT; ++iChannel ) {
		if ( !t3d_vt_add_plan_bytes( rPlan.nVtPhysicalBytes[iChannel], rPlan.nTotalTextureBytes ) ||
		     !t3d_vt_add_plan_bytes( rPlan.nVtTailBytes[iChannel], rPlan.nTotalTextureBytes ) ) {
			return false;
		}
	}

	return t3dCheckedAdd<uint64_t>( rPlan.nTotalBufferBytes, rPlan.nTotalTextureBytes, rPlan.nTotalBytes );
}

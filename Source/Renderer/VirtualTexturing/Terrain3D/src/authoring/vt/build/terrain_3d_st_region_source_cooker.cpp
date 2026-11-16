// Copyright 2026 Terrain3D contributors.
#include "authoring/vt/build/terrain_3d_st_region_source_cooker.h"
#include "authoring/vhm/terrain_3d_vhm_cooker.h"
#include "authoring/vt/cutout/terrain_3d_cutout_cooker.h"
#include "rendering/rd/superterrain/terrain_3d_st_region_package.h"
#include <stddef.h>

static uint64_t t3dStCookHash( uint64_t hash, const uint8_t *data, const size_t bytes )
{
	for ( size_t i = 0u; i < bytes; ++i ) {
		hash = ( hash ^ data[i] ) * UINT64_C( 1099511628211 );
	}
	return hash;
}

bool Terrain3DStRegionSourceCooker::cookCold( const t3dStAuthoredRegionSource_t *pInputs, const uint32_t nCount,
                                              const uint32_t nVtPackageGeneration, std::vector<uint8_t>& rOutput )
{
	if ( pInputs == nullptr || nCount == 0u || nCount > 1024u || nVtPackageGeneration == 0u )
		return false;
	const t3dStAuthoredRegionSource_t *indexed[1024] = {};
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		const t3dStAuthoredRegionSource_t& r = pInputs[i];
		if ( r.m_nRegionIndex >= 1024u || indexed[r.m_nRegionIndex] != nullptr || r.m_pHeightR16 == nullptr || r.m_pControlRF == nullptr ||
		     r.m_pColorRgba8 == nullptr || r.m_nComposedGeneration == 0u || r.m_nComposedAllocation == T3D_ST_INVALID_ID )
			return false;
		indexed[r.m_nRegionIndex] = &r;
	}
	std::vector<std::vector<uint8_t>> vhm( nCount );
	std::vector<std::vector<uint8_t>> mask( nCount );
	std::vector<t3dStPublicationRegionInput_t> inputs( nCount );
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		const t3dStAuthoredRegionSource_t& r = pInputs[i];
		uint64_t sourceSignature = UINT64_C( 1469598103934665603 );
		sourceSignature = t3dStCookHash( sourceSignature, r.m_pHeightR16, 128u * 128u * 2u );
		sourceSignature = t3dStCookHash( sourceSignature, r.m_pControlRF, 128u * 128u * 4u );
		sourceSignature = t3dStCookHash( sourceSignature, r.m_pColorRgba8, 128u * 128u * 4u );
		uint64_t recipeSignature = UINT64_C( 0x5354303656523031 );
		recipeSignature = t3dStCookHash( recipeSignature, ( const uint8_t * )&nVtPackageGeneration, sizeof( nVtPackageGeneration ) );
		recipeSignature = t3dStCookHash( recipeSignature, ( const uint8_t * )&r.m_nComposedGeneration, sizeof( r.m_nComposedGeneration ) );
		t3dStVhmCookInput_t height = {};
		height.m_nRegionIndex = r.m_nRegionIndex;
		height.m_nGeneration = r.m_nComposedGeneration;
		height.m_nSourceSignature = sourceSignature;
		height.m_nRecipeSignature = recipeSignature;
		const int x = ( int )( r.m_nRegionIndex & 31u );
		const int y = ( int )( r.m_nRegionIndex >> 5u );
		for ( int dz = -1; dz <= 1; ++dz ) {
			for ( int dx = -1; dx <= 1; ++dx ) {
				if ( x + dx < 0 || x + dx >= 32 || y + dz < 0 || y + dz >= 32 )
					continue;
				const uint32_t neighbor = ( uint32_t )( ( y + dz ) * 32 + x + dx );
				const t3dStAuthoredRegionSource_t *pNeighbor = indexed[neighbor];
				if ( pNeighbor != nullptr ) {
					// All neighbors are from the same authored cold snapshot and
					// VT package. Per-region COMPOSED generation may differ after
					// incremental material cooks; height gutters still need the
					// actual adjacent CPU height instead of a clamped edge.
					height.m_pHeightLe[dz + 1][dx + 1] = pNeighbor->m_pHeightR16;
					height.m_nNeighborGeneration[dz + 1][dx + 1] = r.m_nComposedGeneration;
				}
			}
		}
		if ( !Terrain3DVhmCooker::cookRegionCold( height, vhm[i] ) )
			return false;
		t3dStCutoutCookInput_t cutout = {};
		cutout.m_pControlLe = r.m_pControlRF;
		cutout.m_pColorRgba8 = r.m_pColorRgba8;
		cutout.m_nRegionIndex = r.m_nRegionIndex;
		cutout.m_nGeneration = r.m_nComposedGeneration;
		cutout.m_nSourceSignature = sourceSignature;
		cutout.m_nRecipeSignature = recipeSignature;
		if ( !Terrain3DCutoutCooker::cookRegionCold( cutout, mask[i] ) )
			return false;
		t3dStPublicationRegionInput_t& input = inputs[i];
		input.m_nRegionIndex = r.m_nRegionIndex;
		input.m_nComposedAllocation = r.m_nComposedAllocation;
		input.m_nGeneration = r.m_nComposedGeneration;
		input.m_nPackedPhysicalFlags = 0x80000000u;
		input.m_nSourceSignature = sourceSignature;
		input.m_nRecipeSignature = recipeSignature;
		input.m_pVhmBytes = vhm[i].data();
		input.m_nVhmBytes = vhm[i].size();
		input.m_pMaskBytes = mask[i].data();
		input.m_nMaskBytes = mask[i].size();
	}
	return Terrain3DStRegionPackage::buildCold( inputs.data(), nCount, nVtPackageGeneration, rOutput );
}

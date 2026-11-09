// Copyright 2026 Terrain3D contributors.
#include "authoring/vt/cutout/terrain_3d_cutout_cooker.h"
#include "rendering/rd/vt/cutout/terrain_3d_cutout_contract.h"
#include "rendering/rd/vhm/terrain_3d_st_sidecar_wire.h"

bool Terrain3DCutoutCooker::cookRegionCold( const t3dStCutoutCookInput_t& rSource, std::vector<uint8_t>& rOutput )
{
	if ( rSource.m_pControlLe == nullptr || rSource.m_pColorRgba8 == nullptr || rSource.m_nRegionIndex >= 1024u ||
	     rSource.m_nGeneration == 0u || rSource.m_nSourceSignature == 0u || rSource.m_nRecipeSignature == 0u ) {
		return false;
	}
	std::vector<uint8_t> payload( T3D_ST_CUTOUT_ENTRIES_BYTES, 0u );
	uint32_t planeWords[T3D_ST_CUTOUT_PAGE_WORDS] = {};
	uint32_t nPinnedWords = 0u;
	for ( uint32_t page = 0u; page < T3D_ST_CUTOUT_PAGES_PER_REGION; ++page ) {
		for ( uint32_t i = 0u; i < T3D_ST_CUTOUT_PAGE_WORDS; ++i ) {
			planeWords[i] = 0u;
		}
		const uint32_t originX = ( page & 1u ) * T3D_ST_CUTOUT_PAGE_SIDE;
		const uint32_t originY = ( page >> 1u ) * T3D_ST_CUTOUT_PAGE_SIDE;
		uint32_t uniform = 0u;
		bool bUniform = true;
		for ( uint32_t y = 0u; y < T3D_ST_CUTOUT_PAGE_SIDE; ++y ) {
			for ( uint32_t x = 0u; x < T3D_ST_CUTOUT_PAGE_SIDE; ++x ) {
				const uint32_t texel = ( originY + y ) * T3D_ST_CUTOUT_SIDE + originX + x;
				const uint32_t value =
				    t3dStCutoutSourceValue( t3dStLoadLe32( rSource.m_pControlLe + texel * 4u ), rSource.m_pColorRgba8[texel * 4u + 3u] );
				if ( x == 0u && y == 0u ) {
					uniform = value;
				} else if ( value != uniform ) {
					bUniform = false;
				}
				const uint32_t bitIndex = y * T3D_ST_CUTOUT_PAGE_SIDE + x;
				for ( uint32_t plane = 0u; plane < T3D_ST_CUTOUT_PLANES; ++plane ) {
					planeWords[plane * T3D_ST_CUTOUT_PLANE_WORDS + ( bitIndex >> 5u )] |= ( ( value >> plane ) & 1u ) << ( bitIndex & 31u );
				}
			}
		}
		const size_t entryOffset = size_t( page ) * T3D_ST_CUTOUT_ENTRY_BYTES;
		t3dStStoreLe32( payload.data() + entryOffset, T3D_ST_CUTOUT_INVALID );
		t3dStStoreLe32( payload.data() + entryOffset + 4u, bUniform ? T3D_ST_CUTOUT_INVALID : nPinnedWords );
		t3dStStoreLe32( payload.data() + entryOffset + 8u, rSource.m_nGeneration );
		t3dStStoreLe32( payload.data() + entryOffset + 12u, T3D_ST_CUTOUT_VALID | ( bUniform ? T3D_ST_CUTOUT_UNIFORM | uniform : 0u ) );
		if ( !bUniform ) {
			const size_t offset = payload.size();
			payload.resize( offset + T3D_ST_CUTOUT_PAGE_BYTES );
			for ( uint32_t i = 0u; i < T3D_ST_CUTOUT_PAGE_WORDS; ++i ) {
				t3dStStoreLe32( payload.data() + offset + size_t( i ) * 4u, planeWords[i] );
			}
			nPinnedWords += T3D_ST_CUTOUT_PAGE_WORDS;
		}
	}
	t3dStSidecarInfo_t info = {};
	info.m_nMagic = T3D_ST_SIDECAR_MASK_MAGIC;
	info.m_nVersion = T3D_ST_SIDECAR_VERSION;
	info.m_nRegionIndex = rSource.m_nRegionIndex;
	info.m_nGeneration = rSource.m_nGeneration;
	info.m_nSourceSignature = rSource.m_nSourceSignature;
	info.m_nRecipeSignature = rSource.m_nRecipeSignature;
	info.m_nEntryCount = T3D_ST_CUTOUT_PAGES_PER_REGION;
	return t3dStWriteSidecar( info, payload, rOutput );
}

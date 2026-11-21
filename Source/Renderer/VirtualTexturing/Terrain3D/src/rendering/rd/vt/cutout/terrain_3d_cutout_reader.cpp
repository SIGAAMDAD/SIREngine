// Copyright 2026 Terrain3D contributors.
#include "rendering/rd/vt/cutout/terrain_3d_cutout_reader.h"
#include "rendering/rd/vhm/terrain_3d_st_sidecar_wire.h"

bool Terrain3DCutoutRegionView::open( const uint8_t *pData, const size_t nBytes, const uint32_t nRegion, const uint32_t nGeneration,
                                      const uint64_t nSourceSignature, const uint64_t nRecipeSignature )
{
	t3dStSidecarInfo_t info = {};
	if ( !t3dStReadSidecar( pData, nBytes, T3D_ST_SIDECAR_MASK_MAGIC, nRegion, nGeneration, nSourceSignature, nRecipeSignature, info ) ||
	     info.m_nEntryCount != T3D_ST_CUTOUT_PAGES_PER_REGION || info.m_nPayloadBytes < T3D_ST_CUTOUT_ENTRIES_BYTES ) {
		return false;
	}
	const uint8_t *p = pData + T3D_ST_SIDECAR_HEADER_BYTES;
	t3dStCutoutEntry_t staged[T3D_ST_CUTOUT_PAGES_PER_REGION] = {};
	uint32_t words = 0u;
	for ( uint32_t page = 0u; page < T3D_ST_CUTOUT_PAGES_PER_REGION; ++page ) {
		const uint8_t *e = p + size_t( page ) * T3D_ST_CUTOUT_ENTRY_BYTES;
		t3dStCutoutEntry_t& r = staged[page];
		r.m_nPhysicalSlot = t3dStLoadLe32( e );
		r.m_nPinnedWordOffset = t3dStLoadLe32( e + 4u );
		r.m_nGeneration = t3dStLoadLe32( e + 8u );
		r.m_nPackedState = t3dStLoadLe32( e + 12u );
		const bool bUniform = ( r.m_nPackedState & T3D_ST_CUTOUT_UNIFORM ) != 0u;
		if ( r.m_nGeneration != nGeneration || r.m_nPhysicalSlot != T3D_ST_CUTOUT_INVALID ||
		     ( r.m_nPackedState & T3D_ST_CUTOUT_VALID ) == 0u || ( r.m_nPackedState & 0x3ffffff8u ) != 0u ||
		     ( bUniform && r.m_nPinnedWordOffset != T3D_ST_CUTOUT_INVALID ) ||
		     ( !bUniform && ( r.m_nPinnedWordOffset != words || ( r.m_nPackedState & 7u ) != 0u ) ) ) {
			return false;
		}
		if ( !bUniform ) {
			words += T3D_ST_CUTOUT_PAGE_WORDS;
		}
	}
	if ( uint64_t( info.m_nPayloadBytes ) != uint64_t( T3D_ST_CUTOUT_ENTRIES_BYTES ) + uint64_t( words ) * 4u ) {
		return false;
	}
	for ( uint32_t i = 0u; i < T3D_ST_CUTOUT_PAGES_PER_REGION; ++i ) {
		m_entries[i] = staged[i];
	}
	m_nRegionIndex = nRegion;
	m_nGeneration = nGeneration;
	m_nPinnedWords = words;
	m_pPayload = p;
	return true;
}

bool Terrain3DCutoutRegionView::sample( const uint32_t nX, const uint32_t nY, const uint32_t nGeneration, uint8_t& rValue ) const
{
	if ( m_pPayload == nullptr || nX >= T3D_ST_CUTOUT_SIDE || nY >= T3D_ST_CUTOUT_SIDE || nGeneration == 0u ||
	     nGeneration != m_nGeneration ) {
		return false;
	}
	const uint32_t page = ( nX >> 6u ) + ( ( nY >> 6u ) << 1u );
	const t3dStCutoutEntry_t& entry = m_entries[page];
	if ( entry.m_nGeneration != nGeneration ) {
		return false;
	}
	if ( ( entry.m_nPackedState & T3D_ST_CUTOUT_UNIFORM ) != 0u ) {
		rValue = uint8_t( entry.m_nPackedState & 7u );
		return true;
	}
	const uint32_t texel = ( ( nY & 63u ) << 6u ) + ( nX & 63u );
	const uint32_t word = entry.m_nPinnedWordOffset + ( texel >> 5u );
	const uint8_t *p = m_pPayload + T3D_ST_CUTOUT_ENTRIES_BYTES + size_t( word ) * 4u;
	uint8_t value = 0u;
	for ( uint32_t plane = 0u; plane < T3D_ST_CUTOUT_PLANES; ++plane ) {
		const uint32_t planeWord = t3dStLoadLe32( p + size_t( plane ) * T3D_ST_CUTOUT_PLANE_WORDS * 4u );
		value |= uint8_t( ( ( planeWord >> ( texel & 31u ) ) & 1u ) << plane );
	}
	rValue = value;
	return true;
}

bool Terrain3DCutoutRegionView::appendGpuPayload( std::vector<uint32_t>& rWords, t3dStCutoutEntry_t *pFourEntries,
                                                  const uint32_t nMaximumWords ) const
{
	if ( m_pPayload == nullptr || pFourEntries == nullptr || rWords.size() > nMaximumWords ||
	     m_nPinnedWords > nMaximumWords - rWords.size() || rWords.size() > UINT32_MAX ) {
		return false;
	}
	const uint32_t base = uint32_t( rWords.size() );
	// One resize per cold transaction. Caller swaps the completed GPU buffers
	// with last-good buffers only after every region has passed validation.
	rWords.resize( size_t( base ) + m_nPinnedWords );
	for ( uint32_t i = 0u; i < m_nPinnedWords; ++i ) {
		rWords[base + i] = t3dStLoadLe32( m_pPayload + T3D_ST_CUTOUT_ENTRIES_BYTES + size_t( i ) * 4u );
	}
	for ( uint32_t page = 0u; page < T3D_ST_CUTOUT_PAGES_PER_REGION; ++page ) {
		pFourEntries[page] = m_entries[page];
		if ( ( m_entries[page].m_nPackedState & T3D_ST_CUTOUT_UNIFORM ) == 0u ) {
			pFourEntries[page].m_nPinnedWordOffset += base;
		}
	}
	return true;
}

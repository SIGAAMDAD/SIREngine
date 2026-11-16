// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_CUTOUT_READER_H
#define TERRAIN3D_CUTOUT_READER_H

#include "rendering/rd/vt/cutout/terrain_3d_cutout_contract.h"
#include <stddef.h>
#include <stdint.h>
#include <vector>

// Validated borrowed immutable region. Caller keeps sidecar bytes alive while
// this view and a publication using its pointers exist.
class Terrain3DCutoutRegionView
{
	const uint8_t *m_pPayload = nullptr;
	t3dStCutoutEntry_t m_entries[T3D_ST_CUTOUT_PAGES_PER_REGION] = {};
	uint32_t m_nGeneration = 0u;
	uint32_t m_nRegionIndex = T3D_ST_CUTOUT_INVALID;
	uint32_t m_nPinnedWords = 0u;

public:
	bool open( const uint8_t *pData, size_t nBytes, uint32_t nRegion, uint32_t nGeneration, uint64_t nSourceSignature,
	           uint64_t nRecipeSignature );
	bool sample( uint32_t nX, uint32_t nY, uint32_t nGeneration, uint8_t& rValue ) const;
	bool valid( void ) const
	{
		return m_pPayload != nullptr;
	}
	uint32_t getPinnedWordCount( void ) const
	{
		return m_nPinnedWords;
	}
	uint32_t getRootIndex( void ) const
	{
		return m_nRegionIndex * T3D_ST_CUTOUT_PAGES_PER_REGION;
	}
	const t3dStCutoutEntry_t& getEntry( uint32_t nPage ) const
	{
		return m_entries[nPage];
	}
	// Cold upload preparation: offsets relocated into a *single* shared pinned
	// bitplane buffer, with no second page residency owner. ST05 binds the SSBO.
	bool appendGpuPayload( std::vector<uint32_t>& rWords, t3dStCutoutEntry_t *pFourEntries, uint32_t nMaximumWords ) const;
};

#endif // TERRAIN3D_CUTOUT_READER_H

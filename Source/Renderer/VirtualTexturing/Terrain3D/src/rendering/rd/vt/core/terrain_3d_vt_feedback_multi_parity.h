// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_PARITY_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_PARITY_H

#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_reducer.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include <stdint.h>
#include <string.h>

// Exact, sample-weighted V2-to-V1 direct-page recall diagnostics.
// Four cold-owned V2 snapshots cover the existing asynchronous feedback ring.
// Neither this cache nor its comparisons admit a page to residency.
#define T3D_VT_MULTI_PARITY_MAX_WORDS ( 240u * 136u )
#define T3D_VT_MULTI_PARITY_TOTAL_WORDS ( T3D_VT_MULTI_PARITY_MAX_WORDS * T3D_VT_FEEDBACK_GRID_SLOT_COUNT )

struct t3dVTFeedbackMultiParitySample_t
{
	uint32_t m_nObserved = 0u;
	uint32_t m_nCovered = 0u;
	uint32_t m_nMissing = 0u;
};

struct t3dVTFeedbackMultiParitySlot_t
{
	t3dVTFeedbackGridHeader_t m_Header{};
	uint32_t m_nWords = 0u;
	bool m_bValid = false;
};

class Terrain3DVTFeedbackMultiParityRing
{
public:
	void bindCold( uint32_t *pWords, const uint32_t nCapacityWords )
	{
		m_pWords = nCapacityWords >= T3D_VT_MULTI_PARITY_TOTAL_WORDS ? pWords : NULL;
		m_nNext = 0u;
		for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
			m_Slots[i] = t3dVTFeedbackMultiParitySlot_t{};
		}
	}

	bool capture( const t3dVTFeedbackGridHeader_t& rHeader, const uint8_t *pWords )
	{
		if ( m_pWords == NULL || pWords == NULL || rHeader.m_nMagic != T3D_VT_FEEDBACK_GRID_MAGIC ||
		     rHeader.m_nVersion != T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION || rHeader.m_nFrame == 0u || rHeader.m_nFeedbackEpoch == 0u ||
		     rHeader.m_nDeviceGeneration == 0u || rHeader.m_nViewportKey == 0u || rHeader.m_nWordCount == 0u ||
		     rHeader.m_nWordCount > T3D_VT_MULTI_PARITY_MAX_WORDS ) {
			return false;
		}
		uint32_t index = T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
		for ( uint32_t i = 0u; i < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++i ) {
			if ( m_Slots[i].m_bValid && sameFrame( m_Slots[i].m_Header, rHeader ) ) {
				index = i;
				break;
			}
		}
		if ( index == T3D_VT_FEEDBACK_GRID_SLOT_COUNT ) {
			index = m_nNext;
			m_nNext = ( m_nNext + 1u ) % T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
		}
		uint32_t *const pDestination = m_pWords + ( size_t )index * T3D_VT_MULTI_PARITY_MAX_WORDS;
		memcpy( pDestination, pWords, ( size_t )rHeader.m_nWordCount * sizeof( uint32_t ) );
		m_Slots[index].m_Header = rHeader;
		m_Slots[index].m_nWords = rHeader.m_nWordCount;
		m_Slots[index].m_bValid = true;
		return true;
	}

	bool compare( const t3dVTFeedbackGridHeader_t& rV1, const uint32_t nPageCount, const Terrain3DVTFeedbackReducer& rV1Reducer,
	              t3dVTFeedbackMultiParitySample_t& rResult ) const
	{
		rResult = t3dVTFeedbackMultiParitySample_t{};
		if ( m_pWords == NULL || nPageCount == 0u || rV1.m_nVersion != T3D_VT_FEEDBACK_MULTI_VERSION ) {
			return false;
		}
		for ( uint32_t slot = 0u; slot < T3D_VT_FEEDBACK_GRID_SLOT_COUNT; ++slot ) {
			const t3dVTFeedbackMultiParitySlot_t& rSlot = m_Slots[slot];
			if ( !rSlot.m_bValid || !sameFrame( rSlot.m_Header, rV1 ) )
				continue;
			const uint32_t *const pPages = m_pWords + ( size_t )slot * T3D_VT_MULTI_PARITY_MAX_WORDS;
			for ( uint32_t i = 0u; i < rSlot.m_nWords; ++i ) {
				const uint32_t page = pPages[i];
				if ( page >= nPageCount )
					continue;
				++rResult.m_nObserved;
				if ( rV1Reducer.hasDirectPage( page ) ) {
					++rResult.m_nCovered;
				} else {
					++rResult.m_nMissing;
				}
			}
			return true;
		}
		return false;
	}

private:
	static bool sameFrame( const t3dVTFeedbackGridHeader_t& rA, const t3dVTFeedbackGridHeader_t& rB )
	{
		return rA.m_nFrame == rB.m_nFrame && rA.m_nFeedbackEpoch == rB.m_nFeedbackEpoch &&
		       rA.m_nDatabaseGeneration == rB.m_nDatabaseGeneration && rA.m_nDeviceGeneration == rB.m_nDeviceGeneration &&
		       rA.m_nViewportKey == rB.m_nViewportKey;
	}

	uint32_t *m_pWords = NULL;
	uint32_t m_nNext = 0u;
	t3dVTFeedbackMultiParitySlot_t m_Slots[T3D_VT_FEEDBACK_GRID_SLOT_COUNT]{};
};

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_PARITY_H

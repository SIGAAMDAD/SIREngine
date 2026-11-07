// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_H

#include <stddef.h>
#include <stdint.h>
#include "terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"

// P31b experimental, separately versioned feedback. A slot is a 40-byte
// feedback header followed by fixed, nine-word records per coarse cell:
// page[4], frequency[4], overflow. The source must contain ALL depth-tested
// virtual-page requests per pixel; this contract does not create that source.
// An overflow or incomplete source rejects the capture, not just the cell.
#define T3D_VT_FEEDBACK_MULTI_VERSION 3u
#define T3D_VT_FEEDBACK_MULTI_PAGES 4u
#define T3D_VT_FEEDBACK_MULTI_RECORD_WORDS 9u
#define T3D_VT_FEEDBACK_MULTI_MAX_LAYERS 8u
#define T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL 256u

struct t3dVTFeedbackMultiRecord_t
{
	uint32_t m_nPages[T3D_VT_FEEDBACK_MULTI_PAGES];
	uint32_t m_nCounts[T3D_VT_FEEDBACK_MULTI_PAGES];
	uint32_t m_nOverflow;
};

struct t3dVTFeedbackMultiParams_t
{
	uint32_t m_nGridWidth;
	uint32_t m_nGridHeight;
	uint32_t m_nViewportWidth;
	uint32_t m_nViewportHeight;
	uint32_t m_nSlot;
	uint32_t m_nSlotStrideWords;
	uint32_t m_nPageCount;
	uint32_t m_nSourceLayers;
	uint32_t m_nPixelLimit;
	uint32_t m_nRecordWords;
	uint32_t m_nSlotWords;
	uint32_t m_nReserved;
};

static_assert( sizeof( t3dVTFeedbackMultiRecord_t ) == 36u, "P31b multi feedback record" );
static_assert( sizeof( t3dVTFeedbackMultiParams_t ) == 48u, "P31b push constant layout" );

inline bool t3d_vt_feedback_multi_layout( const uint64_t nReadbackLimit, t3dVTFeedbackGridLayout_t& rLayout )
{
	const uint32_t dimensions[][2] = { { 240u, 136u }, { 160u, 90u }, { 128u, 72u }, { 32u, 24u } };
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		const uint64_t slotBytes =
		    sizeof( t3dVTFeedbackGridHeader_t ) + ( uint64_t )dimensions[i][0] * dimensions[i][1] * sizeof( t3dVTFeedbackMultiRecord_t );
		if ( slotBytes <= nReadbackLimit && slotBytes <= UINT32_MAX / T3D_VT_FEEDBACK_GRID_SLOT_COUNT ) {
			rLayout = { dimensions[i][0], dimensions[i][1], ( uint32_t )slotBytes,
			            ( uint32_t )( slotBytes * T3D_VT_FEEDBACK_GRID_SLOT_COUNT ) };
			return true;
		}
	}
	rLayout = t3dVTFeedbackGridLayout_t{};
	return false;
}

inline bool t3d_vt_feedback_multi_params( const t3dVTFeedbackGridLayout_t& rLayout, uint32_t nViewportWidth, uint32_t nViewportHeight,
                                          uint32_t nSlot, uint32_t nPageCount, uint32_t nSourceLayers, t3dVTFeedbackMultiParams_t& rParams )
{
	const uint64_t words =
	    ( uint64_t )rLayout.m_nWidth * rLayout.m_nHeight * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS + T3D_VT_FEEDBACK_GRID_HEADER_WORDS;
	if ( rLayout.m_nWidth == 0u || rLayout.m_nHeight == 0u || rLayout.m_nWidth > 240u || rLayout.m_nHeight > 136u || nViewportWidth == 0u ||
	     nViewportHeight == 0u || nViewportWidth > UINT16_MAX || nViewportHeight > UINT16_MAX || nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT ||
	     nPageCount == 0u || nSourceLayers == 0u || nSourceLayers > T3D_VT_FEEDBACK_MULTI_MAX_LAYERS ||
	     ( ( uint64_t )( ( nViewportWidth + rLayout.m_nWidth - 1u ) / rLayout.m_nWidth ) *
	       ( ( nViewportHeight + rLayout.m_nHeight - 1u ) / rLayout.m_nHeight ) ) > T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL ||
	     rLayout.m_nSlotBytes % sizeof( uint32_t ) != 0u || words != rLayout.m_nSlotBytes / sizeof( uint32_t ) )
		return false;
	rParams = { rLayout.m_nWidth,
	            rLayout.m_nHeight,
	            nViewportWidth,
	            nViewportHeight,
	            nSlot,
	            ( uint32_t )words,
	            nPageCount,
	            nSourceLayers,
	            T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL,
	            T3D_VT_FEEDBACK_MULTI_RECORD_WORDS,
	            ( uint32_t )words,
	            0u };
	return true;
}

inline t3dVTFeedbackGridHeader_t t3d_vt_feedback_multi_header( const t3dVTFeedbackMultiParams_t& rParams, uint32_t nFrame, uint32_t nEpoch,
                                                               uint32_t nDatabase, uint32_t nDevice, uint64_t nViewport )
{
	return { T3D_VT_FEEDBACK_GRID_MAGIC,
	         T3D_VT_FEEDBACK_MULTI_VERSION,
	         rParams.m_nSlot,
	         nFrame,
	         nEpoch,
	         nDatabase,
	         nDevice,
	         rParams.m_nGridWidth * rParams.m_nGridHeight * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS,
	         nViewport };
}

inline bool t3d_vt_feedback_multi_validate( const t3dVTFeedbackGridHeader_t& rHeader, const t3dVTFeedbackMultiParams_t& rParams,
                                            uint32_t nFrame, uint32_t nEpoch, uint32_t nDatabase, uint32_t nDevice, uint64_t nViewport )
{
	return rHeader.m_nMagic == T3D_VT_FEEDBACK_GRID_MAGIC && rHeader.m_nVersion == T3D_VT_FEEDBACK_MULTI_VERSION &&
	       rHeader.m_nSlot == rParams.m_nSlot && rHeader.m_nFrame == nFrame && rHeader.m_nFeedbackEpoch == nEpoch &&
	       rHeader.m_nDatabaseGeneration == nDatabase && rHeader.m_nDeviceGeneration == nDevice && rHeader.m_nViewportKey == nViewport &&
	       rHeader.m_nWordCount == rParams.m_nGridWidth * rParams.m_nGridHeight * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS;
}

// V1 must never call t3dVTFeedbackGridSlot_t::complete(), which checks V2.
struct t3dVTFeedbackMultiSlot_t
{
	t3dVTFeedbackGridHeader_t m_Header{};
	t3dVTFeedbackGridSlotStatus_t m_eStatus = T3D_VT_FEEDBACK_SLOT_FREE;
	bool m_bRetireOnCallback = false;

	void reset()
	{
		m_Header = t3dVTFeedbackGridHeader_t{};
		m_eStatus = T3D_VT_FEEDBACK_SLOT_FREE;
		m_bRetireOnCallback = false;
	}
	void invalidate()
	{
		if ( m_eStatus == T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED )
			m_bRetireOnCallback = true;
		else
			reset();
	}
	bool prepare( const t3dVTFeedbackGridHeader_t& rHeader )
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_FREE || rHeader.m_nVersion != T3D_VT_FEEDBACK_MULTI_VERSION )
			return false;
		m_Header = rHeader;
		m_eStatus = T3D_VT_FEEDBACK_SLOT_PREPARED;
		return true;
	}
	bool markDrawn()
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_PREPARED )
			return false;
		m_eStatus = T3D_VT_FEEDBACK_SLOT_DRAWN;
		return true;
	}
	bool submit( bool bAccepted )
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_DRAWN )
			return false;
		if ( bAccepted )
			m_eStatus = T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED;
		else
			reset();
		return bAccepted;
	}
	bool complete( const t3dVTFeedbackGridHeader_t& rReceived, const t3dVTFeedbackMultiParams_t& rParams )
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED ||
		     !t3d_vt_feedback_multi_validate( rReceived, rParams, m_Header.m_nFrame, m_Header.m_nFeedbackEpoch,
		                                      m_Header.m_nDatabaseGeneration, m_Header.m_nDeviceGeneration, m_Header.m_nViewportKey ) )
			return false;
		if ( m_bRetireOnCallback ) {
			reset();
			return false;
		}
		m_eStatus = T3D_VT_FEEDBACK_SLOT_READY;
		return true;
	}
};

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_H

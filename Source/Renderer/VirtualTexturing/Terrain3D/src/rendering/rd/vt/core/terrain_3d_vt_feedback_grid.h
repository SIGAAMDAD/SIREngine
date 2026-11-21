// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_GRID_H
#define TERRAIN3D_VT_FEEDBACK_GRID_H

#include <stddef.h>
#include <stdint.h>

// The grid stores desired GLOBAL virtual-page IDs, never physical cache slots.
#define T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION 2u
#define T3D_VT_FEEDBACK_GRID_MAGIC 0x47465456u
#define T3D_VT_FEEDBACK_GRID_INVALID_PAGE 0xffffffffu
#define T3D_VT_FEEDBACK_GRID_SLOT_COUNT 4u
#define T3D_VT_FEEDBACK_GRID_HEADER_WORDS 10u

typedef uint32_t t3dVTFeedbackWord_t;

typedef struct t3dVTFeedbackGridHeader_s
{
	uint32_t m_nMagic;
	uint32_t m_nVersion;
	uint32_t m_nSlot;
	uint32_t m_nFrame;
	uint32_t m_nFeedbackEpoch;
	uint32_t m_nDatabaseGeneration;
	uint32_t m_nDeviceGeneration;
	uint32_t m_nWordCount;
	uint64_t m_nViewportKey;
} t3dVTFeedbackGridHeader_t;

typedef struct t3dVTFeedbackGridParams_s
{
	uint32_t m_nGridWidth;
	uint32_t m_nAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nFlags;
	uint32_t m_nPredictedTerrainMaterials;
	// Low 16 bits: grid height; high 16: viewport width.
	uint32_t m_nGridHeightAndViewportWidth;
	// Low 16 bits: capture slot; high 16: viewport height.
	uint32_t m_nWriteSlotAndViewportHeight;
	uint32_t m_nSlotStrideWords;
} t3dVTFeedbackGridParams_t;

typedef struct t3dVTFeedbackGridLayout_s
{
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_nSlotBytes;
	uint32_t m_nTotalBytes;
} t3dVTFeedbackGridLayout_t;

static_assert( sizeof( t3dVTFeedbackGridHeader_t ) / sizeof( uint32_t ) == T3D_VT_FEEDBACK_GRID_HEADER_WORDS, "Feedback header word ABI" );
static_assert( sizeof( t3dVTFeedbackGridHeader_t ) == 40u, "Feedback grid header ABI" );
static_assert( offsetof( t3dVTFeedbackGridHeader_t, m_nViewportKey ) == 32u, "Feedback viewport offset" );
static_assert( sizeof( t3dVTFeedbackGridParams_t ) == 32u, "Feedback parameters ABI" );
static_assert( offsetof( t3dVTFeedbackGridParams_t, m_nPredictedTerrainMaterials ) == 16u, "Feedback predicted material mask ABI" );
static_assert( sizeof( t3dVTFeedbackWord_t ) == 4u, "VT feedback must use one 32-bit word" );

inline bool t3d_vt_feedback_grid_make_params( const t3dVTFeedbackGridLayout_t& rLayout, const uint32_t nAllocationCount,
                                              const uint32_t nPageCount, const uint32_t nViewportWidth, const uint32_t nViewportHeight,
                                              const uint32_t nSlot, const uint32_t nMaterialMask, t3dVTFeedbackGridParams_t& rParams )
{
	if ( rLayout.m_nWidth == 0u || rLayout.m_nHeight == 0u || rLayout.m_nHeight > UINT16_MAX || nViewportWidth == 0u ||
	     nViewportWidth > UINT16_MAX || nViewportHeight == 0u || nViewportHeight > UINT16_MAX || nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT ||
	     nAllocationCount == 0u || nPageCount == 0u )
		return false;
	rParams = { rLayout.m_nWidth,
	            nAllocationCount,
	            nPageCount,
	            1u,
	            nMaterialMask,
	            ( nViewportWidth << 16u ) | rLayout.m_nHeight,
	            ( nViewportHeight << 16u ) | nSlot,
	            ( uint32_t )( rLayout.m_nSlotBytes / sizeof( uint32_t ) ) };
	return true;
}

inline bool t3d_vt_feedback_grid_layout( const uint64_t nReadbackLimit, t3dVTFeedbackGridLayout_t& rLayout )
{
	const uint32_t dimensions[][2] = { { 240u, 136u }, { 160u, 90u }, { 128u, 72u }, { 32u, 24u } };
	for ( uint32_t i = 0u; i < 4u; ++i ) {
		const uint64_t nSlotBytes =
		    sizeof( t3dVTFeedbackGridHeader_t ) + ( uint64_t )dimensions[i][0] * dimensions[i][1] * sizeof( t3dVTFeedbackWord_t );
		if ( nSlotBytes <= nReadbackLimit && nSlotBytes <= UINT32_MAX / T3D_VT_FEEDBACK_GRID_SLOT_COUNT ) {
			rLayout.m_nWidth = dimensions[i][0];
			rLayout.m_nHeight = dimensions[i][1];
			rLayout.m_nSlotBytes = ( uint32_t )nSlotBytes;
			rLayout.m_nTotalBytes = ( uint32_t )nSlotBytes * T3D_VT_FEEDBACK_GRID_SLOT_COUNT;
			return true;
		}
	}
	rLayout = t3dVTFeedbackGridLayout_t{};
	return false;
}

inline bool t3d_vt_feedback_grid_validate( const t3dVTFeedbackGridHeader_t& rHeader, const uint32_t nSlot, const uint32_t nFrame,
                                           const uint32_t nEpoch, const uint32_t nDatabase, const uint32_t nDevice,
                                           const uint64_t nViewport, const uint32_t nExpectedWords )
{
	return rHeader.m_nMagic == T3D_VT_FEEDBACK_GRID_MAGIC && rHeader.m_nVersion == T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION &&
	       rHeader.m_nSlot == nSlot && rHeader.m_nFrame == nFrame && rHeader.m_nFeedbackEpoch == nEpoch &&
	       rHeader.m_nDatabaseGeneration == nDatabase && rHeader.m_nDeviceGeneration == nDevice && rHeader.m_nViewportKey == nViewport &&
	       rHeader.m_nWordCount == nExpectedWords;
}

enum t3dVTFeedbackGridSlotStatus_t : uint8_t
{
	T3D_VT_FEEDBACK_SLOT_FREE = 0u,
	T3D_VT_FEEDBACK_SLOT_PREPARED,
	T3D_VT_FEEDBACK_SLOT_DRAWN,
	T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED,
	T3D_VT_FEEDBACK_SLOT_READY
};

struct t3dVTFeedbackGridSlot_t
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
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_FREE )
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
	bool submit( const bool bAccepted )
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_DRAWN )
			return false;
		if ( bAccepted )
			m_eStatus = T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED;
		else
			reset(); // This frame's feedback is skipped. Prediction remains authoritative.
		return bAccepted;
	}
	bool complete( const t3dVTFeedbackGridHeader_t& rReceived )
	{
		if ( m_eStatus != T3D_VT_FEEDBACK_SLOT_READBACK_SUBMITTED ||
		     !t3d_vt_feedback_grid_validate( rReceived, m_Header.m_nSlot, m_Header.m_nFrame, m_Header.m_nFeedbackEpoch,
		                                     m_Header.m_nDatabaseGeneration, m_Header.m_nDeviceGeneration, m_Header.m_nViewportKey,
		                                     m_Header.m_nWordCount ) )
			return false;
		if ( m_bRetireOnCallback ) {
			reset();
			return false;
		}
		m_eStatus = T3D_VT_FEEDBACK_SLOT_READY;
		return true;
	}
};

#endif // TERRAIN3D_VT_FEEDBACK_GRID_H

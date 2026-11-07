// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_SINGLE_WRITER_H
#define TERRAIN3D_VT_SINGLE_WRITER_H

#include <stdint.h>

#include "terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"

// Experimental P31a input contract (not a shipping feedback producer):
// binding 0 is a fully depth-tested R32_UINT virtual-page attachment with
// exactly one resolved page ID per viewport pixel. Uncovered pixels contain
// UINT32_MAX. The image must be complete and visible to compute before dispatch.
// binding 1 is the existing version-2 four-slot feedback grid SSBO. The
// dispatch writes one page per cell and leaves all 40 header bytes untouched.
// Missing/multiple-material requests may differ from raster SSBO feedback:
// this path MUST remain disabled until device capture and residency parity.
#define T3D_VT_SINGLE_WRITER_CONTRACT_VERSION 1u
#define T3D_VT_SINGLE_WRITER_SAMPLE_AXIS 4u
#define T3D_VT_SINGLE_WRITER_LOCAL_SIZE 8u

typedef struct t3dVTSingleWriterParams_s
{
	uint32_t m_nGridWidth;
	uint32_t m_nGridHeight;
	uint32_t m_nViewportWidth;
	uint32_t m_nViewportHeight;
	uint32_t m_nSlot;
	uint32_t m_nSlotStrideWords;
	uint32_t m_nPageCount;
	uint32_t m_nReserved;
} t3dVTSingleWriterParams_t;

static_assert( sizeof( t3dVTSingleWriterParams_t ) == 32u, "P31a compute push-constant ABI" );

inline bool t3d_vt_single_writer_params( const t3dVTFeedbackGridLayout_t& rLayout, const uint32_t nViewportWidth,
                                         const uint32_t nViewportHeight, const uint32_t nSlot, const uint32_t nPageCount,
                                         t3dVTSingleWriterParams_t& rResult )
{
	if ( rLayout.m_nWidth == 0u || rLayout.m_nHeight == 0u || rLayout.m_nWidth > 240u || rLayout.m_nHeight > 136u || nViewportWidth == 0u ||
	     nViewportHeight == 0u || nViewportWidth > UINT16_MAX || nViewportHeight > UINT16_MAX || nSlot >= T3D_VT_FEEDBACK_GRID_SLOT_COUNT ||
	     nPageCount == 0u || rLayout.m_nSlotBytes % sizeof( uint32_t ) != 0u ||
	     rLayout.m_nSlotBytes / sizeof( uint32_t ) < T3D_VT_FEEDBACK_GRID_HEADER_WORDS + rLayout.m_nWidth * rLayout.m_nHeight )
		return false;
	rResult = { rLayout.m_nWidth,
	            rLayout.m_nHeight,
	            nViewportWidth,
	            nViewportHeight,
	            nSlot,
	            ( uint32_t )( rLayout.m_nSlotBytes / sizeof( uint32_t ) ),
	            nPageCount,
	            0u };
	return true;
}

#endif // TERRAIN3D_VT_SINGLE_WRITER_H

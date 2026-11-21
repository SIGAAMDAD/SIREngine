// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_READER_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_READER_H

#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_reducer.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// P31b experimental reader. The caller owns the slot readback and reducer
// scratch storage. A stale, truncated, malformed or overflowed capture is
// rejected without admitting partial page demand. The existing v2 reducer
// and fragment-producer policy are left untouched.
inline bool t3d_vt_feedback_multi_reduce_capture( const uint8_t *pSlot, const size_t nSlotBytes, const t3dVTFeedbackMultiParams_t& rParams,
                                                  uint32_t nFrame, uint32_t nEpoch, uint32_t nDatabase, uint32_t nDevice,
                                                  uint64_t nViewport, Terrain3DVTFeedbackReducer& rReducer,
                                                  t3dVTFeedbackPageLookup_t pLookup, void *pUser, const t3dVTFeedbackPolicy_t& rPolicy,
                                                  t3dVTFeedbackReductionStats_t& rStats )
{
	rStats = t3dVTFeedbackReductionStats_t{};
	rReducer.discardCapture();
	const uint64_t nCellCount = ( uint64_t )rParams.m_nGridWidth * rParams.m_nGridHeight;
	const uint64_t expectedWords = T3D_VT_FEEDBACK_GRID_HEADER_WORDS + nCellCount * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS;
	if ( !pSlot || rParams.m_nGridWidth == 0u || rParams.m_nGridWidth > 240u || rParams.m_nGridHeight == 0u ||
	     rParams.m_nGridHeight > 136u || rParams.m_nPageCount == 0u || rParams.m_nRecordWords != T3D_VT_FEEDBACK_MULTI_RECORD_WORDS ||
	     rParams.m_nSourceLayers == 0u || rParams.m_nSourceLayers > T3D_VT_FEEDBACK_MULTI_MAX_LAYERS ||
	     rParams.m_nPixelLimit != T3D_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL || rParams.m_nSlotWords != expectedWords ||
	     rParams.m_nSlotStrideWords != expectedWords || nSlotBytes != expectedWords * sizeof( uint32_t ) )
		return false;
	t3dVTFeedbackGridHeader_t header{};
	memcpy( &header, pSlot, sizeof( header ) );
	if ( !t3d_vt_feedback_multi_validate( header, rParams, nFrame, nEpoch, nDatabase, nDevice, nViewport ) )
		return false;
	return rReducer.reduceMulti( pSlot + sizeof( header ), ( uint32_t )nCellCount, rParams.m_nPageCount, pLookup, pUser, rPolicy, rStats );
}

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_READER_H

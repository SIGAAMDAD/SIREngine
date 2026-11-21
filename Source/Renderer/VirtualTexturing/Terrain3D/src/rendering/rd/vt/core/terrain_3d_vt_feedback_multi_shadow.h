// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_SHADOW_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_SHADOW_H

#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include <stdint.h>

// Diagnostics-only V1 capture inspection. This contract cannot
// authorize a V1 page for streaming: production V2 remains authoritative.
struct t3dVTFeedbackMultiShadowStats_t
{
	uint64_t m_nAccepted = 0u;
	uint64_t m_nRejected = 0u;
	uint64_t m_nStale = 0u;
	uint64_t m_nSamples = 0u;
	uint64_t m_nValidSamples = 0u;
	uint64_t m_nUniquePages = 0u;
	uint64_t m_nCandidates = 0u;
	uint64_t m_nPairedCaptures = 0u;
	uint64_t m_nUnpairedCaptures = 0u;
	uint64_t m_nV2ObservedSamples = 0u;
	uint64_t m_nV2CoveredSamples = 0u;
	uint64_t m_nV2MissingSamples = 0u;
};

inline bool t3d_vt_feedback_multi_shadow_frame_valid( const t3dVTFeedbackGridHeader_t& rHeader, const t3dVTFeedbackMultiParams_t& rParams,
                                                      uint32_t nCurrentFrame, uint32_t nCurrentEpoch, uint32_t nDatabase, uint32_t nDevice,
                                                      uint64_t nViewport )
{
	return rHeader.m_nFrame != 0u && rHeader.m_nFrame <= nCurrentFrame && rHeader.m_nFeedbackEpoch == nCurrentEpoch &&
	       nCurrentEpoch != 0u &&
	       t3d_vt_feedback_multi_validate( rHeader, rParams, rHeader.m_nFrame, nCurrentEpoch, nDatabase, nDevice, nViewport );
}

inline void t3d_vt_feedback_multi_shadow_accept( t3dVTFeedbackMultiShadowStats_t& rStats, uint32_t nSamples, uint32_t nValidSamples,
                                                 uint32_t nUniquePages, uint32_t nCandidates )
{
	++rStats.m_nAccepted;
	rStats.m_nSamples += nSamples;
	rStats.m_nValidSamples += nValidSamples;
	rStats.m_nUniquePages += nUniquePages;
	rStats.m_nCandidates += nCandidates;
}

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_SHADOW_H

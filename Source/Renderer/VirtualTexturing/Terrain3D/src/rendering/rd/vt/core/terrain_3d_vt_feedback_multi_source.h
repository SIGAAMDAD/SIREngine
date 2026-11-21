// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_SOURCE_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_SOURCE_H

#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi.h"
#include <stdint.h>

// P31c: The producer has to certify the ENTIRE visible page-demand set.
// These bits describe participating renderer families, not shader variants.
#define T3D_VT_MULTI_SOURCE_TERRAIN ( 1u << 0u )
#define T3D_VT_MULTI_SOURCE_WORLD ( 1u << 1u )
#define T3D_VT_MULTI_SOURCE_INSTANCES ( 1u << 2u )
#define T3D_VT_MULTI_SOURCE_TRANSPARENT ( 1u << 3u )
#define T3D_VT_MULTI_SOURCE_ALL ( ( 1u << 4u ) - 1u )

struct t3dVTFeedbackMultiSourceProof_t
{
	uint64_t m_nViewport = 0u;
	uint32_t m_nFrame = 0u;
	uint32_t m_nFeedbackEpoch = 0u;
	uint32_t m_nDatabaseGeneration = 0u;
	uint32_t m_nDeviceGeneration = 0u;
	uint32_t m_nViewportWidth = 0u;
	uint32_t m_nViewportHeight = 0u;
	uint32_t m_nLayerCount = 0u;
	uint32_t m_nRequiredFamilies = 0u;
	uint32_t m_nPublishedFamilies = 0u;
	bool m_bDepthTested = false;
	bool m_bVisibilityComplete = false;
	bool m_bSamplerInputsPublished = false;
};

inline bool t3d_vt_feedback_multi_source_complete( const t3dVTFeedbackMultiSourceProof_t& rProof, const t3dVTFeedbackMultiParams_t& rParams,
                                                   const uint32_t nEpoch, const uint32_t nDatabase, const uint32_t nDevice,
                                                   const uint64_t nViewport )
{
	return rProof.m_nViewport == nViewport && nViewport != 0u && rProof.m_nFeedbackEpoch == nEpoch && nEpoch != 0u &&
	       rProof.m_nDatabaseGeneration == nDatabase && rProof.m_nDeviceGeneration == nDevice && nDevice != 0u &&
	       rProof.m_nViewportWidth == rParams.m_nViewportWidth && rProof.m_nViewportHeight == rParams.m_nViewportHeight &&
	       rProof.m_nLayerCount == rParams.m_nSourceLayers &&
	       ( rProof.m_nRequiredFamilies & ( T3D_VT_MULTI_SOURCE_TERRAIN | T3D_VT_MULTI_SOURCE_WORLD | T3D_VT_MULTI_SOURCE_INSTANCES ) ) ==
	           ( T3D_VT_MULTI_SOURCE_TERRAIN | T3D_VT_MULTI_SOURCE_WORLD | T3D_VT_MULTI_SOURCE_INSTANCES ) &&
	       ( rProof.m_nRequiredFamilies & ~T3D_VT_MULTI_SOURCE_ALL ) == 0u &&
	       ( rProof.m_nPublishedFamilies & rProof.m_nRequiredFamilies ) == rProof.m_nRequiredFamilies &&
	       ( rProof.m_nPublishedFamilies & ~T3D_VT_MULTI_SOURCE_ALL ) == 0u && rProof.m_bDepthTested && rProof.m_bVisibilityComplete &&
	       rProof.m_bSamplerInputsPublished;
}

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_SOURCE_H

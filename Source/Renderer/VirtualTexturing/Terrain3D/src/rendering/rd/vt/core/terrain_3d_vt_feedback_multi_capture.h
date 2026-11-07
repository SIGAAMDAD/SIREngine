// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_MULTI_CAPTURE_H
#define TERRAIN3D_VT_FEEDBACK_MULTI_CAPTURE_H

#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_source.h"
#include <stddef.h>
#include <stdint.h>

// P31d: CPU replay input for testing the experimental V1 GPU reduction chain.
// Page IDs are layer-major: pages[layer * (width * height) + pixel]. Counts are
// pixel-major. Unused slots MUST contain UINT32_MAX, including empty pixels.
// An engine raster producer must supply a complete depth-resolved capture;
// this view is not evidence that scene visibility has been resolved.
struct t3dVTFeedbackMultiCaptureView_t
{
	const uint32_t *m_pPages = nullptr;
	const uint32_t *m_pCounts = nullptr;
	uint32_t m_nWidth = 0u;
	uint32_t m_nHeight = 0u;
	uint32_t m_nLayers = 0u;
	uint32_t m_nPageCount = 0u;
};

#define T3D_VT_FEEDBACK_MULTI_REPLAY_MAX_BYTES ( 128ull * 1024ull * 1024ull )

inline bool t3d_vt_feedback_multi_capture_bytes( const uint32_t nWidth, const uint32_t nHeight, const uint32_t nLayers,
                                                 uint64_t& rPixelBytes, uint64_t& rTotalBytes )
{
	rPixelBytes = 0u;
	rTotalBytes = 0u;
	if ( nWidth == 0u || nHeight == 0u || nLayers == 0u || nLayers > T3D_VT_FEEDBACK_MULTI_MAX_LAYERS )
		return false;
	const uint64_t bytes = ( uint64_t )nWidth * nHeight * sizeof( uint32_t );
	const uint64_t total = bytes * ( ( uint64_t )nLayers + 1u );
	if ( bytes > INT32_MAX || total > T3D_VT_FEEDBACK_MULTI_REPLAY_MAX_BYTES )
		return false;
	rPixelBytes = bytes;
	rTotalBytes = total;
	return true;
}

inline bool t3d_vt_feedback_multi_capture_valid( const t3dVTFeedbackMultiCaptureView_t& rCapture,
                                                 const t3dVTFeedbackMultiParams_t& rParams )
{
	uint64_t pixelBytes = 0u, totalBytes = 0u;
	if ( !rCapture.m_pPages || !rCapture.m_pCounts ||
	     !t3d_vt_feedback_multi_capture_bytes( rCapture.m_nWidth, rCapture.m_nHeight, rCapture.m_nLayers, pixelBytes, totalBytes ) ||
	     rCapture.m_nWidth != rParams.m_nViewportWidth || rCapture.m_nHeight != rParams.m_nViewportHeight ||
	     rCapture.m_nLayers != rParams.m_nSourceLayers || rCapture.m_nPageCount != rParams.m_nPageCount || rCapture.m_nPageCount == 0u )
		return false;
	const uint64_t pixels = pixelBytes / sizeof( uint32_t );
	for ( uint64_t i = 0u; i < pixels; ++i ) {
		const uint32_t count = rCapture.m_pCounts[i];
		if ( count > rCapture.m_nLayers )
			return false;
		for ( uint32_t layer = 0u; layer < rCapture.m_nLayers; ++layer ) {
			const uint32_t page = rCapture.m_pPages[( uint64_t )layer * pixels + i];
			if ( layer < count ) {
				if ( page >= rCapture.m_nPageCount )
					return false;
			} else if ( page != UINT32_MAX ) {
				return false;
			}
		}
	}
	return true;
}

inline bool t3d_vt_feedback_multi_same_source_frame( const t3dVTFeedbackMultiSourceProof_t& rA, const t3dVTFeedbackMultiSourceProof_t& rB )
{
	return rA.m_nViewport == rB.m_nViewport && rA.m_nFrame == rB.m_nFrame && rA.m_nFeedbackEpoch == rB.m_nFeedbackEpoch &&
	       rA.m_nDatabaseGeneration == rB.m_nDatabaseGeneration && rA.m_nDeviceGeneration == rB.m_nDeviceGeneration &&
	       rA.m_nViewportWidth == rB.m_nViewportWidth && rA.m_nViewportHeight == rB.m_nViewportHeight &&
	       rA.m_nLayerCount == rB.m_nLayerCount && rA.m_nRequiredFamilies == rB.m_nRequiredFamilies &&
	       rA.m_nPublishedFamilies == rB.m_nPublishedFamilies && rA.m_bDepthTested == rB.m_bDepthTested &&
	       rA.m_bVisibilityComplete == rB.m_bVisibilityComplete && rA.m_bSamplerInputsPublished == rB.m_bSamplerInputsPublished;
}

#endif // TERRAIN3D_VT_FEEDBACK_MULTI_CAPTURE_H

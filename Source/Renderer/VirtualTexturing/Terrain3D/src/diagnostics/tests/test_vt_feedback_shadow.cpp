// Copyright (c) 2023-2026 Terrain3D contributors.
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_shadow.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_multi_reader.h"
#include <assert.h>
#include <string.h>
#include <vector>

struct Page_t
{
	uint32_t parent;
	uint16_t mip;
};
static bool lookup( void *pData, uint32_t nPage, uint32_t& rParent, uint16_t& rMip )
{
	if ( nPage >= 16u )
		return false;
	const Page_t *const pages = ( const Page_t * )pData;
	rParent = pages[nPage].parent;
	rMip = pages[nPage].mip;
	return true;
}
int main()
{
	t3dVTFeedbackGridLayout_t layout{};
	assert( t3d_vt_feedback_multi_layout( 30000u, layout ) );
	t3dVTFeedbackMultiParams_t params{};
	assert( t3d_vt_feedback_multi_params( layout, 160u, 90u, 2u, 16u, 4u, params ) );
	const uint64_t viewport = 0x12341234u;
	const t3dVTFeedbackGridHeader_t header = t3d_vt_feedback_multi_header( params, 100u, 44u, 5u, 17u, viewport );
	assert( t3d_vt_feedback_multi_shadow_frame_valid( header, params, 100u, 44u, 5u, 17u, viewport ) );
	assert( t3d_vt_feedback_multi_shadow_frame_valid( header, params, 104u, 44u, 5u, 17u, viewport ) );
	for ( uint32_t i = 0u; i < 1000u; ++i ) {
		const uint32_t frame = 1u + ( i * 1664525u + 1013904223u ) % 200u;
		const bool expected = frame >= 100u;
		assert( t3d_vt_feedback_multi_shadow_frame_valid( header, params, frame, 44u, 5u, 17u, viewport ) == expected );
	}
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, params, 99u, 44u, 5u, 17u, viewport ) );
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, params, 100u, 45u, 5u, 17u, viewport ) );
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, params, 100u, 44u, 6u, 17u, viewport ) );
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, params, 100u, 44u, 5u, 18u, viewport ) );
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, params, 100u, 44u, 5u, 17u, viewport + 1u ) );
	t3dVTFeedbackMultiParams_t wrongSlot = params;
	wrongSlot.m_nSlot = 1u;
	assert( !t3d_vt_feedback_multi_shadow_frame_valid( header, wrongSlot, 100u, 44u, 5u, 17u, viewport ) );

	std::vector<uint32_t> words( params.m_nSlotWords, UINT32_MAX );
	memcpy( words.data(), &header, sizeof( header ) );
	const uint32_t count = params.m_nGridWidth * params.m_nGridHeight;
	for ( uint32_t i = 0u; i < count; ++i ) {
		const uint32_t at = T3D_VT_FEEDBACK_GRID_HEADER_WORDS + i * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS;
		for ( uint32_t j = 0u; j < 4u; ++j )
			words[at + 4u + j] = 0u;
		words[at + 8u] = 0u;
	}
	words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS] = 3u;
	words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS + 4u] = 4u;
	Page_t pages[16]{};
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		pages[i].parent = UINT32_MAX;
		pages[i].mip = 0u;
	}
	uint32_t epochs[16]{}, direct[16]{}, aggregate[16]{}, touched[16]{};
	t3dVTFeedbackCandidate_t candidates[16]{};
	Terrain3DVTFeedbackReducer reducer;
	reducer.bindCold( 16u, 16u, epochs, direct, aggregate, touched, candidates );
	t3dVTFeedbackPolicy_t policy{};
	policy.m_nMinimumSamples = 1u;
	policy.m_nMaxCandidates = 16u;
	t3dVTFeedbackReductionStats_t reductions{};
	const uint32_t legacy[2] = { 2u, 2u };
	assert( reducer.reduce( ( const uint8_t * )legacy, 2u, 16u, lookup, pages, policy, reductions ) );
	assert( reducer.getSelectedCount() == 1u );
	// The runtime consumes/stages V2 results before doing diagnostics-only V1.
	const uint32_t productionStagedPage = reducer.getSelected( 0u ).m_nPage;
	uint32_t productionStagedCount = 1u;
	assert( t3d_vt_feedback_multi_reduce_capture( ( const uint8_t * )words.data(), words.size() * sizeof( uint32_t ), params, 100u, 44u, 5u,
	                                              17u, viewport, reducer, lookup, pages, policy, reductions ) );
	t3dVTFeedbackMultiShadowStats_t stats{};
	t3d_vt_feedback_multi_shadow_accept( stats, reductions.m_nSamples, reductions.m_nValidSamples, reductions.m_nDirectUniquePages,
	                                     reductions.m_nCandidates );
	assert( stats.m_nAccepted == 1u && stats.m_nSamples == 4u );
	assert( stats.m_nValidSamples == 4u && stats.m_nUniquePages == 1u );
	assert( productionStagedPage == 2u && productionStagedCount == 1u );
	reducer.discardCapture();
	assert( reducer.getSelectedCount() == 0u && reducer.getTouchedCount() == 0u );
	++stats.m_nStale;
	++stats.m_nRejected;
	assert( stats.m_nStale == 1u && stats.m_nRejected == 1u );
	return 0;
}

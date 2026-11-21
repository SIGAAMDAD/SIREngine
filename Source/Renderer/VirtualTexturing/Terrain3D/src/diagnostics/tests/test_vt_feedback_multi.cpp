#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_multi_reader.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

struct Page_t
{
	uint32_t m_nParent;
	uint16_t m_nMip;
};
static bool lookup( void *pUser, uint32_t nPage, uint32_t& rParent, uint16_t& rMip )
{
	if ( nPage >= 8u )
		return false;
	const Page_t *const pages = ( const Page_t * )pUser;
	rParent = pages[nPage].m_nParent;
	rMip = pages[nPage].m_nMip;
	return true;
}
static uint32_t frequency( const Terrain3DVTFeedbackReducer& rReducer, uint32_t nPage )
{
	for ( uint32_t i = 0u; i < rReducer.getSelectedCount(); ++i )
		if ( rReducer.getSelected( i ).m_nPage == nPage )
			return rReducer.getSelected( i ).m_nFrequency;
	return 0u;
}

int main()
{
	t3dVTFeedbackGridLayout_t small{}, medium{}, high{};
	assert( !t3d_vt_feedback_multi_layout( 4096u, small ) );
	assert( t3d_vt_feedback_multi_layout( 30000u, small ) );
	assert( small.m_nWidth == 32u && small.m_nHeight == 24u );
	assert( t3d_vt_feedback_multi_layout( 350000u, medium ) );
	assert( medium.m_nWidth == 128u && medium.m_nHeight == 72u );
	assert( t3d_vt_feedback_multi_layout( 1200000u, high ) );
	assert( high.m_nWidth == 240u && high.m_nHeight == 136u );
	assert( high.m_nTotalBytes == high.m_nSlotBytes * T3D_VT_FEEDBACK_GRID_SLOT_COUNT );
	t3dVTFeedbackMultiParams_t params{};
	assert( t3d_vt_feedback_multi_params( small, 160u, 90u, 1u, 8u, 8u, params ) );
	assert( params.m_nSlotStrideWords == small.m_nSlotBytes / 4u );
	assert( params.m_nPixelLimit == 256u && params.m_nRecordWords == 9u );
	assert( !t3d_vt_feedback_multi_params( small, 65536u, 90u, 1u, 8u, 8u, params ) );
	assert( !t3d_vt_feedback_multi_params( small, 1920u, 1080u, 1u, 8u, 8u, params ) );
	assert( !t3d_vt_feedback_multi_params( small, 160u, 90u, 4u, 8u, 8u, params ) );
	assert( !t3d_vt_feedback_multi_params( small, 160u, 90u, 1u, 8u, 9u, params ) );
	assert( t3d_vt_feedback_multi_params( small, 160u, 90u, 1u, 8u, 8u, params ) );

	Page_t pages[8] = { { UINT32_MAX, 3u }, { 0u, 2u }, { 1u, 1u }, { 1u, 1u },
	                    { UINT32_MAX, 3u }, { 4u, 2u }, { 5u, 1u }, { UINT32_MAX, 0u } };
	uint32_t epochs[8]{}, direct[8]{}, aggregates[8]{}, touched[8]{};
	t3dVTFeedbackCandidate_t candidates[8]{};
	Terrain3DVTFeedbackReducer reducer;
	reducer.bindCold( 8u, 8u, epochs, direct, aggregates, touched, candidates );
	t3dVTFeedbackPolicy_t policy{};
	policy.m_nMinimumSamples = 1u;
	policy.m_nMaxCandidates = 8u;
	t3dVTFeedbackReductionStats_t stats{};

	// The live reader accepts a slot-sized byte view and never allocates.
	uint32_t *words = new uint32_t[params.m_nSlotStrideWords];
	for ( uint32_t i = 0u; i < params.m_nSlotStrideWords; ++i )
		words[i] = UINT32_MAX;
	const t3dVTFeedbackGridHeader_t header = t3d_vt_feedback_multi_header( params, 101u, 7u, 3u, 4u, 0x1122334455667788ull );
	memcpy( words, &header, sizeof( header ) );
	for ( uint32_t i = 0u; i < params.m_nGridWidth * params.m_nGridHeight; ++i ) {
		const uint32_t off = T3D_VT_FEEDBACK_GRID_HEADER_WORDS + i * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS;
		for ( uint32_t j = 0u; j < T3D_VT_FEEDBACK_MULTI_PAGES; ++j )
			words[off + 4u + j] = 0u;
		words[off + 8u] = 0u;
	}
	words[10u] = 2u;
	words[11u] = 3u;
	words[14u] = 15u;
	words[15u] = 5u;
	words[19u] = 6u;
	words[23u] = 2u;
	const size_t bytes = params.m_nSlotStrideWords * 4u;
	const auto reduce = [&]() -> bool {
		return t3d_vt_feedback_multi_reduce_capture( ( const uint8_t * )words, bytes, params, 101u, 7u, 3u, 4u, 0x1122334455667788ull,
		                                             reducer, lookup, pages, policy, stats );
	};
	assert( reduce() );
	assert( stats.m_nValidSamples == 22u && stats.m_nDirectUniquePages == 3u );
	assert( frequency( reducer, 2u ) == 15u && frequency( reducer, 3u ) == 5u );
	assert( frequency( reducer, 1u ) == 20u && frequency( reducer, 0u ) == 20u );
	assert( frequency( reducer, 6u ) == 2u );
	assert( !t3d_vt_feedback_multi_reduce_capture( ( const uint8_t * )words, bytes - 4u, params, 101u, 7u, 3u, 4u, 0x1122334455667788ull,
	                                               reducer, lookup, pages, policy, stats ) );
	assert( !t3d_vt_feedback_multi_reduce_capture( ( const uint8_t * )words, bytes, params, 101u, 8u, 3u, 4u, 0x1122334455667788ull,
	                                               reducer, lookup, pages, policy, stats ) );
	words[18u] = 1u; // first cell overflow: reject the entire capture
	assert( !reduce() );
	assert( reducer.getSelectedCount() == 0u );
	words[18u] = 0u;
	words[15u] = 0u; // zero weight for an occupied slot: malformed
	assert( !reduce() );
	words[15u] = 5u;
	words[11u] = 2u; // duplicate material IDs within one cell: malformed
	assert( !reduce() );
	words[11u] = 3u;
	words[14u] = 2049u; // impossible sample count
	assert( !reduce() );
	words[14u] = 15u;
	assert( reduce() );

	// Legacy v2 must still accept exactly one word per cell unchanged.
	const uint32_t legacyWords[3] = { 2u, 2u, 3u };
	assert( reducer.reduce( ( const uint8_t * )legacyWords, 3u, 8u, lookup, pages, policy, stats ) );
	assert( stats.m_nValidSamples == 3u && stats.m_nDirectUniquePages == 2u );
	delete[] words;
	return 0;
}

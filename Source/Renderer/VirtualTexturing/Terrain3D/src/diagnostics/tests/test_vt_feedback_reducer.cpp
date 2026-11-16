#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_reducer.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

struct Page_t
{
	uint32_t parent;
	uint16_t mip;
};
static bool lookup( void *ctx, uint32_t page, uint32_t& parent, uint16_t& mip )
{
	const Page_t *const pages = ( const Page_t * )ctx;
	if ( page >= 8u )
		return false;
	parent = pages[page].parent;
	mip = pages[page].mip;
	return true;
}
static uint32_t frequency( const Terrain3DVTFeedbackReducer& reducer, uint32_t page )
{
	for ( uint32_t i = 0u; i < reducer.getSelectedCount(); ++i )
		if ( reducer.getSelected( i ).m_nPage == page )
			return reducer.getSelected( i ).m_nFrequency;
	return 0u;
}
int main()
{
	Page_t pages[8] = { { UINT32_MAX, 3u }, { 0u, 2u }, { 1u, 1u }, { 1u, 1u }, { UINT32_MAX, 3u }, { 4u, 2u }, { 5u, 1u }, { 70u, 0u } };
	uint32_t epochs[8]{}, direct[8]{}, aggregates[8]{}, touched[8]{};
	t3dVTFeedbackCandidate_t candidates[8]{};
	Terrain3DVTFeedbackReducer reducer;
	reducer.bindCold( 8u, 8u, epochs, direct, aggregates, touched, candidates );
	t3dVTFeedbackPolicy_t policy{};
	policy.m_nMaxCandidates = 8u;
	t3dVTFeedbackReductionStats_t stats{};
	const uint32_t samples[] = { 2u, 2u, 2u, 3u, 3u, 6u, UINT32_MAX, 998u };
	assert( reducer.reduce( ( const uint8_t * )samples, 8u, 8u, lookup, pages, policy, stats ) );
	assert( stats.m_nValidSamples == 6u && stats.m_nUniquePages == 7u );
	// P05: three page IDs were directly observed; the remaining unique pages
	// came from the CPU reducer's parent promotion policy.
	assert( stats.m_nDirectUniquePages == 3u );
	assert( frequency( reducer, 2u ) == 3u && frequency( reducer, 3u ) == 2u );
	assert( frequency( reducer, 1u ) == 5u && frequency( reducer, 0u ) == 5u );
	assert( frequency( reducer, 4u ) == 1u && frequency( reducer, 6u ) == 0u );
	assert( stats.m_nRejectedNoise >= 2u );
	assert( reducer.getSelected( 0u ).m_nPage == 0u );
	for ( uint32_t i = 0u; i < reducer.getSelectedCount(); ++i ) {
		for ( uint32_t j = i + 1u; j < reducer.getSelectedCount(); ++j )
			assert( reducer.getSelected( i ).m_nPage != reducer.getSelected( j ).m_nPage );
	}
	policy.m_nMaxCandidates = 2u;
	assert( reducer.reduce( ( const uint8_t * )samples, 8u, 8u, lookup, pages, policy, stats ) );
	assert( reducer.getSelectedCount() == 2u );
	const uint32_t steady = reducer.getSelected( 0u ).m_nScore;
	policy.m_nMotionLevel = 2u;
	assert( reducer.reduce( ( const uint8_t * )samples, 8u, 8u, lookup, pages, policy, stats ) );
	assert( reducer.getSelected( 0u ).m_nScore > steady );
	const uint32_t noise[] = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
	assert( reducer.reduce( ( const uint8_t * )noise, 3u, 8u, lookup, pages, policy, stats ) );
	assert( reducer.getSelectedCount() == 0u && reducer.getTouchedCount() == 0u );
	assert( stats.m_nDirectUniquePages == 0u );
	// Cycles and bad parent references must terminate within the bounded traversal.
	pages[0].parent = 1u;
	pages[7].parent = 100u;
	const uint32_t corrupt[] = { 2u, 2u, 7u, 7u };
	assert( reducer.reduce( ( const uint8_t * )corrupt, 4u, 8u, lookup, pages, policy, stats ) );
	assert( stats.m_nInvalidParents != 0u );
	// Determinism for repeated captures with the same source identity.
	uint32_t resultA[8]{}, resultB[8]{};
	const uint32_t firstCount = reducer.getSelectedCount();
	for ( uint32_t i = 0u; i < firstCount; ++i )
		resultA[i] = reducer.getSelected( i ).m_nPage;
	assert( reducer.reduce( ( const uint8_t * )corrupt, 4u, 8u, lookup, pages, policy, stats ) );
	for ( uint32_t i = 0u; i < reducer.getSelectedCount(); ++i )
		resultB[i] = reducer.getSelected( i ).m_nPage;
	assert( firstCount == reducer.getSelectedCount() && memcmp( resultA, resultB, sizeof( resultA ) ) == 0 );
	return 0;
}

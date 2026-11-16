// Copyright (c) 2023-2026 Terrain3D contributors.
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_multi_parity.h"
#include "rendering/rd/vt/core/terrain_3d_vt_feedback_grid.h"
#include "rendering/rd/vt/runtime/terrain_3d_vt_feedback_multi_reader.h"
#include <assert.h>
#include <string.h>
#include <vector>

struct t3dParityTestPage_t
{
	uint32_t m_nParent;
	uint16_t m_nMip;
};
static bool t3dParityLookup( void *pUser, uint32_t nPage, uint32_t& rParent, uint16_t& rMip )
{
	if ( nPage >= 32u )
		return false;
	const t3dParityTestPage_t *const pPages = ( const t3dParityTestPage_t * )pUser;
	rParent = pPages[nPage].m_nParent;
	rMip = pPages[nPage].m_nMip;
	return true;
}

int main()
{
	std::vector<uint32_t> memory( T3D_VT_MULTI_PARITY_TOTAL_WORDS + 2u, 0x12345678u );
	Terrain3DVTFeedbackMultiParityRing ring;
	ring.bindCold( memory.data() + 1u, T3D_VT_MULTI_PARITY_TOTAL_WORDS );
	t3dVTFeedbackGridLayout_t layout{};
	assert( t3d_vt_feedback_multi_layout( 32000u, layout ) );
	t3dVTFeedbackMultiParams_t params{};
	assert( t3d_vt_feedback_multi_params( layout, 160u, 90u, 0u, 32u, 4u, params ) );
	const uint64_t viewport = 0x1234000000abcull;
	const uint32_t epoch = 7u, database = 9u, device = 10u;
	t3dVTFeedbackGridHeader_t v3Header = t3d_vt_feedback_multi_header( params, 100u, epoch, database, device, viewport );
	t3dVTFeedbackGridHeader_t v2Header = v3Header;
	v2Header.m_nVersion = T3D_VT_FEEDBACK_GRID_CONTRACT_VERSION;
	v2Header.m_nWordCount = 6u;
	const uint32_t v2Words[6] = { 2u, 3u, 4u, 2u, UINT32_MAX, 1000u };
	assert( ring.capture( v2Header, ( const uint8_t * )v2Words ) );
	assert( memory.front() == 0x12345678u && memory.back() == 0x12345678u );

	t3dParityTestPage_t pages[32]{};
	for ( uint32_t i = 0u; i < 32u; ++i )
		pages[i] = { UINT32_MAX, 0u };
	pages[4u].m_nParent = 3u; // Page 3 is only a promoted ancestor.
	uint32_t epochs[32]{}, direct[32]{}, aggregate[32]{}, touched[32]{};
	t3dVTFeedbackCandidate_t candidates[32]{};
	Terrain3DVTFeedbackReducer reducer;
	reducer.bindCold( 32u, 32u, epochs, direct, aggregate, touched, candidates );
	std::vector<uint32_t> v3Words( params.m_nSlotWords, 0u );
	memcpy( v3Words.data(), &v3Header, sizeof( v3Header ) );
	const uint32_t cells = params.m_nGridWidth * params.m_nGridHeight;
	for ( uint32_t i = 0u; i < cells; ++i ) {
		const uint32_t start = T3D_VT_FEEDBACK_GRID_HEADER_WORDS + i * T3D_VT_FEEDBACK_MULTI_RECORD_WORDS;
		for ( uint32_t j = 0u; j < T3D_VT_FEEDBACK_MULTI_PAGES; ++j ) {
			v3Words[start + j] = UINT32_MAX;
		}
	}
	v3Words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS] = 2u;
	v3Words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS + 4u] = 5u;
	v3Words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS + T3D_VT_FEEDBACK_MULTI_RECORD_WORDS] = 4u;
	v3Words[T3D_VT_FEEDBACK_GRID_HEADER_WORDS + T3D_VT_FEEDBACK_MULTI_RECORD_WORDS + 4u] = 3u;
	t3dVTFeedbackPolicy_t policy{};
	policy.m_nMinimumSamples = 1u;
	policy.m_nMaxCandidates = 32u;
	t3dVTFeedbackReductionStats_t reduction{};
	assert( t3d_vt_feedback_multi_reduce_capture( ( const uint8_t * )v3Words.data(), v3Words.size() * sizeof( uint32_t ), params,
	                                              v3Header.m_nFrame, epoch, database, device, viewport, reducer, t3dParityLookup, pages,
	                                              policy, reduction ) );
	assert( reducer.hasDirectPage( 2u ) && reducer.hasDirectPage( 4u ) );
	assert( !reducer.hasDirectPage( 3u ) );
	t3dVTFeedbackMultiParitySample_t result{};
	assert( ring.compare( v3Header, 32u, reducer, result ) );
	assert( result.m_nObserved == 4u && result.m_nCovered == 3u && result.m_nMissing == 1u );
	assert( memory.front() == 0x12345678u && memory.back() == 0x12345678u );

	// Epoch, device, database and viewport identity may not be conflated.
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		t3dVTFeedbackGridHeader_t other = v3Header;
		if ( i == 0u )
			++other.m_nFrame;
		if ( i == 1u )
			++other.m_nFeedbackEpoch;
		if ( i == 2u )
			++other.m_nDatabaseGeneration;
		if ( i == 3u )
			++other.m_nDeviceGeneration;
		if ( i == 4u )
			++other.m_nViewportKey;
		assert( !ring.compare( other, 32u, reducer, result ) );
		assert( result.m_nObserved == 0u && result.m_nMissing == 0u );
	}
	assert( !ring.capture( v2Header, NULL ) );
	t3dVTFeedbackGridHeader_t invalid = v2Header;
	invalid.m_nWordCount = T3D_VT_MULTI_PARITY_MAX_WORDS + 1u;
	assert( !ring.capture( invalid, ( const uint8_t * )v2Words ) );

	// Four snapshots; a fifth replaces only the oldest, with no heap growth.
	for ( uint32_t i = 0u; i < 5u; ++i ) {
		t3dVTFeedbackGridHeader_t frame = v2Header;
		frame.m_nFrame = 200u + i;
		frame.m_nWordCount = 1u;
		const uint32_t sample = i % 2u == 0u ? 2u : 4u;
		assert( ring.capture( frame, ( const uint8_t * )&sample ) );
	}
	t3dVTFeedbackGridHeader_t evicted = v3Header;
	evicted.m_nFrame = 200u;
	assert( !ring.compare( evicted, 32u, reducer, result ) );
	for ( uint32_t frame = 201u; frame <= 204u; ++frame ) {
		evicted.m_nFrame = frame;
		assert( ring.compare( evicted, 32u, reducer, result ) );
		assert( result.m_nObserved == 1u && result.m_nCovered == 1u && result.m_nMissing == 0u );
	}

	// Randomized frame-matched direct recall, including duplicate and invalid V2 pages.
	uint32_t seed = 0x8ac235d1u;
	for ( uint32_t iteration = 0u; iteration < 1400u; ++iteration ) {
		uint32_t wordSet[32]{};
		uint32_t expectedObserved = 0u, expectedCovered = 0u;
		seed = seed * 1664525u + 1013904223u;
		const uint32_t count = 1u + ( seed % 32u );
		for ( uint32_t i = 0u; i < count; ++i ) {
			seed = seed * 1664525u + 1013904223u;
			const uint32_t page = seed % 43u;
			wordSet[i] = page;
			if ( page < 32u ) {
				++expectedObserved;
				if ( page == 2u || page == 4u )
					++expectedCovered;
			}
		}
		t3dVTFeedbackGridHeader_t currentV2 = v2Header;
		currentV2.m_nFrame = 5000u + iteration;
		currentV2.m_nWordCount = count;
		assert( ring.capture( currentV2, ( const uint8_t * )wordSet ) );
		t3dVTFeedbackGridHeader_t currentV1 = v3Header;
		currentV1.m_nFrame = currentV2.m_nFrame;
		assert( ring.compare( currentV1, 32u, reducer, result ) );
		assert( result.m_nObserved == expectedObserved && result.m_nCovered == expectedCovered );
		assert( result.m_nMissing == expectedObserved - expectedCovered );
	}
	assert( memory.front() == 0x12345678u && memory.back() == 0x12345678u );

	// Rebinding clears old identities; optional runtime builds carry no storage.
	ring.bindCold( NULL, 0u );
	assert( !ring.compare( v3Header, 32u, reducer, result ) );
	const uint32_t legacyWords[] = { 2u };
	assert( reducer.reduce( ( const uint8_t * )legacyWords, 1u, 32u, t3dParityLookup, pages, policy, reduction ) );
	assert( reducer.hasDirectPage( 2u ) && !reducer.hasDirectPage( 4u ) );
	reducer.discardCapture();
	assert( !reducer.hasDirectPage( 2u ) );
	return 0;
}

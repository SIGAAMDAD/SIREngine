/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureFeedbackReducer.h>

#include <stdio.h>
#include <vector>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool sirVTTestParents( void *pUser, uint32_t nPage, uint32_t *pParent, uint16_t *pMip ) noexcept
{
	const uint32_t *pParents = ( const uint32_t * )pUser;
	if ( pParents == nullptr || nPage >= 8 || pParent == nullptr || pMip == nullptr ) {
		return false;
	}
	*pParent = pParents[nPage];
	*pMip = ( uint16_t )nPage;
	return true;
}

static bool testFeedback( void )
{
	const uint32_t szParents[8] = { 2, 2, 4, 4, UINT32_MAX, UINT32_MAX, UINT32_MAX, 6 };
	std::vector<uint32_t> epoch( 8, 0 ), direct( 8, 0 ), aggregate( 8, 0 ), touched( 8, 0 );
	std::vector<sirVirtualTextureFeedbackCandidate_t> candidates( 4 );
	sirVirtualTextureFeedbackReducerBuffers_t storage = { epoch.data(), direct.data(), aggregate.data(), touched.data(),
	                                                      candidates.data() };
	SIRVirtualTextureFeedbackReducer reducer;
	SIR_TEST( !reducer.bindCold( {}, 8, 4 ) );
	SIR_TEST( reducer.bindCold( storage, 8, 4 ) );
	const uint32_t words[] = { 0, 0, 1, 2, 3, 6, 6, 7, UINT32_MAX };
	sirVirtualTextureFeedbackReduceInput_t request = { ( const uint8_t * )words, sizeof( words ),    9, 8,
	                                                   sirVTTestParents,         ( void * )szParents };
	sirVirtualTextureFeedbackPolicy_t policy = {};
	sirVirtualTextureFeedbackReductionStats_t stats = {};
	SIR_TEST( reducer.reduce( request, policy, &stats ) );
	SIR_TEST( stats.nSamples == 9 && stats.nValidSamples == 8 && stats.nDirectUniquePages == 6 );
	SIR_TEST( stats.nParentPromotions > 0 && reducer.getSelectedCount() == 4 );
	SIR_TEST( reducer.hasDirectPage( 0 ) && !reducer.hasDirectPage( 4 ) );
	SIR_TEST( reducer.getTouchedPage( 100 ) == UINT32_MAX && reducer.getSelected( 100 ) == nullptr );
	for ( uint32_t i = 1; i < reducer.getSelectedCount(); ++i ) {
		const auto *pPrevious = reducer.getSelected( i - 1 );
		const auto *pCurrent = reducer.getSelected( i );
		SIR_TEST( pPrevious->nScore >= pCurrent->nScore );
	}
	request.nAvailableBytes--;
	SIR_TEST( !reducer.reduce( request, policy, &stats ) && reducer.getSelectedCount() == 0 );
	request.nAvailableBytes++;
	sirVirtualTextureFeedbackMultiRecord_t multi = {};
	for ( uint32_t i = 0; i < SIR_VT_FEEDBACK_MULTI_PAGES; ++i ) {
		multi.szPages[i] = UINT32_MAX;
	}
	multi.szPages[0] = 0;
	multi.szCounts[0] = 4;
	multi.szPages[1] = 1;
	multi.szCounts[1] = 3;
	request = { ( const uint8_t * )&multi, sizeof( multi ), 1, 8, sirVTTestParents, ( void * )szParents };
	SIR_TEST( reducer.reduceMulti( request, policy, &stats ) );
	SIR_TEST( stats.nSamples == 7 && stats.nDirectUniquePages == 2 && reducer.getSelectedCount() != 0 );
	multi.nOverflow = 1;
	SIR_TEST( !reducer.reduceMulti( request, policy, &stats ) && reducer.getSelectedCount() == 0 );
	multi.nOverflow = 0;
	multi.szPages[1] = 0;
	SIR_TEST( !reducer.reduceMulti( request, policy, &stats ) && reducer.getSelectedCount() == 0 );
	multi.szPages[1] = 1;
	multi.szCounts[1] = 2049;
	SIR_TEST( !reducer.reduceMulti( request, policy, &stats ) && reducer.getSelectedCount() == 0 );
	reducer.resetCold();
	SIR_TEST( !reducer.reduceMulti( request, policy, &stats ) );
	return true;
}

int main( void )
{
	return testFeedback() ? 0 : 1;
}

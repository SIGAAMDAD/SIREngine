/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Visibility/SIRVisibilityFoundation.h>

#include <gtest/gtest.h>
#include <limits>

static sirHiZLayout_t testLayout( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	sirHiZLayout_t layout = {};
	EXPECT_EQ( sirHiZCalculateLayout( nWidth, nHeight, &layout ), SIR_GRAPHICS_SUCCESS );
	return layout;
}

static uint32_t testNextRandom( uint32_t *pSeed ) noexcept
{
	*pSeed = *pSeed * 1664525U + 1013904223U;
	return *pSeed;
}

TEST( SIRHiZFoundation, NPOTPyramidMatchesIndependentMinimum )
{
	const sirHiZLayout_t layout = testLayout( 7, 5 );


	const float szDepth[35] = { 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.9f, 0.8f, 0.7f, 0.6f,
	                            0.5f, 0.4f, 0.3f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.0f };
	float szPyramid[128] = {};
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 35, 7 }, szPyramid, 128 ), SIR_GRAPHICS_SUCCESS );
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		const sirHiZMip_t mip = layout.szMips[iMip];
		const uint64_t nScale = 1ULL << iMip;
		for ( uint32_t nY = 0; nY < mip.nHeight; ++nY ) {
			for ( uint32_t nX = 0; nX < mip.nWidth; ++nX ) {
				float fExpected = 1.0f;
				for ( uint64_t nSourceY = ( uint64_t )nY * nScale; nSourceY < ( uint64_t )( nY + 1 ) * nScale && nSourceY < 5;
				      ++nSourceY ) {
					for ( uint64_t nSourceX = ( uint64_t )nX * nScale; nSourceX < ( uint64_t )( nX + 1 ) * nScale && nSourceX < 7;
					      ++nSourceX ) {
						const float fDepth = szDepth[nSourceY * 7 + nSourceX];
						if ( fDepth < fExpected ) {
							fExpected = fDepth;
						}
					}
				}
				EXPECT_FLOAT_EQ( szPyramid[mip.nOffset + ( uint64_t )nY * mip.nWidth + nX], fExpected );
			}
		}
	}
}

TEST( SIRHiZFoundation, NaNInfinityAndNegativeDepthDoNotWrite )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	float szDepth[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
	float szOutput[8] = { -7.0f, -7.0f, -7.0f, -7.0f, -7.0f, -7.0f, -7.0f, -7.0f };
	const float szInvalid[4] = { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -0.1f,
	                             -std::numeric_limits<float>::infinity() };
	for ( float fInvalid : szInvalid ) {
		szDepth[3] = fInvalid;
		EXPECT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szOutput, 8 ), SIR_GRAPHICS_INVALID_ARGUMENT );
		for ( float fValue : szOutput ) {
			EXPECT_EQ( fValue, -7.0f );
		}
	}
	szDepth[3] = 1.1f;
	EXPECT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szOutput, 8 ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( szOutput[0], -7.0f );
}

TEST( SIRHiZFoundation, OverlappingDepthAndOutputFailBeforeWrites )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	float szDepth[32] = { 0.7f, 0.6f, 0.5f, 0.4f };
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szDepth + 1, 31 ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_FLOAT_EQ( szDepth[0], 0.7f );
	EXPECT_FLOAT_EQ( szDepth[1], 0.6f );
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth + 1, 4, 2 }, szDepth, 32 ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_FLOAT_EQ( szDepth[1], 0.6f );
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szDepth + 8, 24 ), SIR_GRAPHICS_SUCCESS );
	EXPECT_FLOAT_EQ( szDepth[8], 0.7f );
	EXPECT_FLOAT_EQ( szDepth[12], 0.4f );
}

TEST( SIRHiZFoundation, OddEdgeChildAndReverseZStrictness )
{
	const sirHiZLayout_t layout = testLayout( 3, 1 );
	const float szDepth[3] = { 0.9f, 0.9f, 0.0f };
	float szPyramid[16] = {};
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 3, 3 }, szPyramid, 16 ), SIR_GRAPHICS_SUCCESS );
	EXPECT_EQ( layout.nMipCount, 3U );
	EXPECT_FLOAT_EQ( szPyramid[3], 0.9f );
	EXPECT_FLOAT_EQ( szPyramid[4], 0.0f );
	EXPECT_FLOAT_EQ( szPyramid[5], 0.0f );
	sirHiZTest_t query = { 0, 0, 2, 1, 0.8f, 0.0f, false, 0 };
	ASSERT_EQ( sirHiZTestRectangle( layout, szPyramid, 16, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_TRUE( query.bOccluded );
	query.fNearestDepth = 0.9f;
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 16, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_FALSE( query.bOccluded );
	query.fNearestDepth = 0.8f;
	query.nMaxX = 3;
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 16, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_FALSE( query.bOccluded );
}

TEST( SIRHiZFoundation, InvalidPyramidAndOffViewportFailOpen )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	float szPyramid[8] = { 0.9f, 0.9f, 0.9f, 0.9f, 0.9f };
	sirHiZTest_t query = { 0, 0, 2, 2, 0.3f, 0.0f, true, 0 };
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 8, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_TRUE( query.bOccluded );
	query.nMaxX = 3;
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 8, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_FALSE( query.bOccluded );
	query.nMaxX = 2;
	szPyramid[2] = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 8, &query ), SIR_GRAPHICS_SUCCESS );
	EXPECT_FALSE( query.bOccluded );
	szPyramid[2] = 0.9f;
	query.fNearestDepth = std::numeric_limits<float>::infinity();
	EXPECT_EQ( sirHiZTestRectangle( layout, szPyramid, 8, &query ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_FALSE( query.bOccluded );
}

TEST( SIRHiZFoundation, MissingHistoryMakesEveryCandidateVisible )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	sirVisibilityCandidate_t szCandidates[2] = {};
	szCandidates[0] = { 17, { 0, 0, 1, 1, 0.5f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 18, { 1, 1, 2, 2, 0.5f, 0.0f, false, 0 }, false };
	uint32_t szVisible[2] = { 0, 0 };
	sirVisibilityCullDesc_t desc = { &layout, nullptr, 0, szCandidates, 2, false };
	sirVisibilityOutput_t output = { szVisible, 2, 77, nullptr, 0, 99 };
	ASSERT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_SUCCESS );
	EXPECT_EQ( output.nVisibleCount, 2U );
	EXPECT_EQ( output.nRejectedCount, 0U );
	EXPECT_EQ( szVisible[0], 17U );
	EXPECT_EQ( szVisible[1], 18U );
	desc.bHistoryValid = true;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_INVALID_ARGUMENT );
}

TEST( SIRHiZFoundation, CapacityFailureIsTransactional )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	const float szDepth[4] = { 0.9f, 0.9f, 0.9f, 0.1f };
	float szPyramid[8] = {};
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szPyramid, 8 ), SIR_GRAPHICS_SUCCESS );
	sirVisibilityCandidate_t szCandidates[2] = {};
	szCandidates[0] = { 11, { 0, 0, 1, 1, 0.2f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 12, { 1, 1, 2, 2, 0.2f, 0.0f, false, 0 }, false };
	sirVisibilityCullDesc_t desc = { &layout, szPyramid, 8, szCandidates, 2, true };
	uint32_t szVisible[2] = { 123, 123 };
	uint32_t szRejected[2] = { 456, 456 };
	sirVisibilityOutput_t output = { szVisible, 0, 78, szRejected, 2, 79 };
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_CAPACITY_EXCEEDED );
	EXPECT_EQ( output.nVisibleCount, 78U );
	EXPECT_EQ( output.nRejectedCount, 79U );
	EXPECT_EQ( szVisible[0], 123U );
	EXPECT_EQ( szRejected[0], 456U );
	output.nVisibleCapacity = 2;
	output.nRejectedCapacity = 0;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_CAPACITY_EXCEEDED );
	EXPECT_EQ( szVisible[0], 123U );
	EXPECT_EQ( szRejected[0], 456U );
}

TEST( SIRHiZFoundation, OverlappingOutputRangesAreRejected )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	const float szDepth[4] = { 0.9f, 0.9f, 0.9f, 0.1f };
	float szPyramid[8] = {};
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szPyramid, 8 ), SIR_GRAPHICS_SUCCESS );
	sirVisibilityCandidate_t szCandidates[2] = {};
	szCandidates[0] = { 11, { 0, 0, 1, 1, 0.2f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 12, { 1, 1, 2, 2, 0.2f, 0.0f, false, 0 }, false };
	sirVisibilityCullDesc_t desc = { &layout, szPyramid, 8, szCandidates, 2, true };
	uint32_t szIds[3] = { 13, 14, 15 };
	sirVisibilityOutput_t output = { szIds, 2, 88, szIds, 2, 89 };
	ASSERT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( output.nVisibleCount, 88U );
	EXPECT_EQ( output.nRejectedCount, 89U );
	EXPECT_EQ( szIds[0], 13U );
	EXPECT_EQ( szIds[1], 14U );
	output.pRejectedIds = szIds + 1;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_SUCCESS );
	EXPECT_EQ( szIds[0], 12U );
	EXPECT_EQ( szIds[1], 11U );
}

TEST( SIRHiZFoundation, ImmutableInputAliasingRejected )
{
	const sirHiZLayout_t layout = testLayout( 2, 2 );
	const float szDepth[4] = { 0.9f, 0.9f, 0.9f, 0.1f };
	float szPyramid[8] = {};
	ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szPyramid, 8 ), SIR_GRAPHICS_SUCCESS );
	sirVisibilityCandidate_t szCandidates[2] = {};
	szCandidates[0] = { 11, { 0, 0, 1, 1, 0.2f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 12, { 1, 1, 2, 2, 0.2f, 0.0f, false, 0 }, false };
	sirVisibilityCullDesc_t desc = { &layout, szPyramid, 8, szCandidates, 2, true };
	uint32_t szVisible[2] = { 0, 0 };
	uint32_t szRejected[2] = { 0, 0 };
	sirVisibilityOutput_t output = { &szCandidates[1].nInstanceId, 2, 33, szRejected, 2, 44 };
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( szCandidates[1].nInstanceId, 12U );
	EXPECT_EQ( output.nVisibleCount, 33U );
	output.pVisibleIds = ( uint32_t * )szPyramid;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_FLOAT_EQ( szPyramid[0], 0.9f );
	output.pVisibleIds = szVisible;
	output.pRejectedIds = &szCandidates[0].nInstanceId;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_INVALID_ARGUMENT );
	EXPECT_EQ( output.nRejectedCount, 44U );
	output.pRejectedIds = szRejected;
	EXPECT_EQ( sirVisibilityCullReference( desc, &output ), SIR_GRAPHICS_SUCCESS );
}

TEST( SIRHiZFoundation, HistoryRequiresConsecutiveMatchingView )
{
	sirVisibilityView_t previous = {};
	previous.nViewId = 4;
	previous.nFrameIndex = 17;
	previous.nProjectionRevision = 7;
	previous.nWorldOriginRevision = 8;
	previous.nDepthProducerRevision = 9;
	previous.nSceneResetRevision = 10;
	previous.nDeviceGeneration = 1;
	previous.nWidth = 1280;
	previous.nHeight = 720;
	previous.nSampleCount = 1;
	previous.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	previous.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	sirVisibilityView_t current = previous;
	current.nFrameIndex++;
	ASSERT_TRUE( sirVisibilityCanReuseHistory( previous, current ) );
	current.nWorldOriginRevision++;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex++;
	current.nDepthProducerRevision++;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex++;
	current.nSceneResetRevision++;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex++;
	current.nDeviceGeneration++;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex++;
	current.bCameraCut = true;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex += 2;
	EXPECT_FALSE( sirVisibilityCanReuseHistory( previous, current ) );
}

TEST( SIRHiZFoundation, GPURequirementsAndDisabledFallback )
{
	sirGraphicsCapabilities_t caps = {};
	sirVisibilityMethodDesc_t method = {};
	method.eMode = SIR_VISIBILITY_HIZ_TWO_PHASE;
	method.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	method.nMaximumCandidates = 16;
	method.nMaximumDraws = 16;
	EXPECT_EQ( sirVisibilityValidateMethod( method, caps ), SIR_GRAPHICS_UNSUPPORTED );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	EXPECT_EQ( sirVisibilityValidateMethod( method, caps ), SIR_GRAPHICS_SUCCESS );
	method.eDepthConvention = SIR_VISIBILITY_FORWARD_Z;
	EXPECT_EQ( sirVisibilityValidateMethod( method, caps ), SIR_GRAPHICS_UNSUPPORTED );
	method.eMode = SIR_VISIBILITY_AUTO;
	EXPECT_EQ( sirVisibilityValidateMethod( method, caps ), SIR_GRAPHICS_INVALID_ARGUMENT );
	method.eMode = SIR_VISIBILITY_DISABLED;
	EXPECT_EQ( sirVisibilityValidateMethod( method, caps ), SIR_GRAPHICS_SUCCESS );
}

TEST( SIRHiZFoundation, RandomizedReferenceNeverFalseOccludes )
{
	uint32_t nSeed = 0x23451234U;
	for ( uint32_t iCase = 0; iCase < 128; ++iCase ) {
		const uint32_t nWidth = 1 + testNextRandom( &nSeed ) % 8;
		const uint32_t nHeight = 1 + testNextRandom( &nSeed ) % 8;
		const sirHiZLayout_t layout = testLayout( nWidth, nHeight );
		float szDepth[64] = {};
		float szPyramid[128] = {};
		for ( uint32_t i = 0; i < nWidth * nHeight; ++i ) {
			szDepth[i] = ( float )( testNextRandom( &nSeed ) % 1001 ) / 1000.0f;
		}
		ASSERT_EQ( sirHiZBuildReference( layout, { szDepth, 64, nWidth }, szPyramid, 128 ), SIR_GRAPHICS_SUCCESS );
		for ( uint32_t iQuery = 0; iQuery < 32; ++iQuery ) {
			const uint32_t nX = testNextRandom( &nSeed ) % nWidth;
			const uint32_t nY = testNextRandom( &nSeed ) % nHeight;
			sirHiZTest_t query = { nX,
			                       nY,
			                       nX + 1 + testNextRandom( &nSeed ) % ( nWidth - nX ),
			                       nY + 1 + testNextRandom( &nSeed ) % ( nHeight - nY ),
			                       0.4f,
			                       0.002f,
			                       false,
			                       0 };
			ASSERT_EQ( sirHiZTestRectangle( layout, szPyramid, 128, &query ), SIR_GRAPHICS_SUCCESS );
			if ( query.bOccluded ) {
				for ( uint32_t nSampleY = query.nMinY; nSampleY < query.nMaxY; ++nSampleY ) {
					for ( uint32_t nSampleX = query.nMinX; nSampleX < query.nMaxX; ++nSampleX ) {
						EXPECT_GT( szDepth[( uint64_t )nSampleY * nWidth + nSampleX], query.fNearestDepth + query.fBias );
					}
				}
			}
		}
	}
}

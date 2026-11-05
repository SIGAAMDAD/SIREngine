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

#include <stdio.h>
#include <vector>

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAILED: %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                      \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( false )

static bool testMethodContracts( void ) noexcept
{
	sirGraphicsCapabilities_t capabilities = {};
	capabilities.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	sirVisibilityMethodDesc_t method = {};
	method.eMode = SIR_VISIBILITY_HIZ_TWO_PHASE;
	method.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	method.nMaximumCandidates = 1024;
	method.nMaximumDraws = 128;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_SUCCESS );
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_SUCCESS );
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_SUCCESS );
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	method.eMode = ( sirVisibilityMode_t )99;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.eMode = SIR_VISIBILITY_AUTO;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.eMode = SIR_VISIBILITY_DISABLED;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_SUCCESS );
	method.eMode = SIR_VISIBILITY_HIERARCHICAL;
	method.nMaximumCandidates = 0;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.nMaximumCandidates = 100;
	method.nMaximumDraws = 0;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.nMaximumDraws = 10;
	method.eDepthConvention = SIR_VISIBILITY_FORWARD_Z;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_UNSUPPORTED );
	method.eDepthConvention = ( sirVisibilityDepthConvention_t )99;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	method.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	capabilities.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_UNSUPPORTED );
	capabilities.nFeatureMask |= SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	method.eMode = SIR_VISIBILITY_HIZ_TEMPORAL;
	REQUIRE( sirVisibilityValidateMethod( method, capabilities ) == SIR_GRAPHICS_SUCCESS );
	return true;
}

static bool testHistoryContracts( void ) noexcept
{
	sirVisibilityView_t previous = {};
	previous.nViewId = 1;
	previous.nFrameIndex = 7;
	previous.nProjectionRevision = 3;
	previous.nWorldOriginRevision = 4;
	previous.nDepthProducerRevision = 5;
	previous.nSceneResetRevision = 6;
	previous.nDeviceGeneration = 9;
	previous.nWidth = 1920;
	previous.nHeight = 1080;
	previous.nSampleCount = 1;
	previous.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	previous.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	sirVisibilityView_t current = previous;
	current.nFrameIndex = 8;
	REQUIRE( sirVisibilityCanReuseHistory( previous, current ) );
	current.bCameraCut = true;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex = 8;
	previous.bCameraCut = true;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.bCameraCut = false;
	previous.nViewId = 0;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nViewId = 1;
	current.nFrameIndex = 9;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nFrameIndex = UINT64_MAX;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nFrameIndex = 7;
	current.nFrameIndex = 8;
	current.nViewId++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nViewId = previous.nViewId;
	current.nProjectionRevision++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current = previous;
	current.nFrameIndex = 8;
	current.nWorldOriginRevision++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nWorldOriginRevision = previous.nWorldOriginRevision;
	current.nDepthProducerRevision++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nDepthProducerRevision = previous.nDepthProducerRevision;
	current.nSceneResetRevision++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nSceneResetRevision = previous.nSceneResetRevision;
	current.nDeviceGeneration++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nDeviceGeneration = previous.nDeviceGeneration;
	previous.nDeviceGeneration = 0;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nDeviceGeneration = current.nDeviceGeneration;
	current.nWidth++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nWidth = previous.nWidth;
	current.nHeight++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nHeight = previous.nHeight;
	current.nSampleCount++;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.nSampleCount = previous.nSampleCount;
	previous.nSampleCount = 0;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nSampleCount = current.nSampleCount;
	current.eDepthFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.eDepthFormat = previous.eDepthFormat;
	current.eDepthConvention = SIR_VISIBILITY_FORWARD_Z;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	current.eDepthConvention = previous.eDepthConvention;
	previous.eDepthConvention = SIR_VISIBILITY_FORWARD_Z;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	previous.nWidth = 0;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nWidth = current.nWidth;
	previous.nHeight = 0;
	REQUIRE( !sirVisibilityCanReuseHistory( previous, current ) );
	previous.nHeight = current.nHeight;
	REQUIRE( sirVisibilityCanReuseHistory( previous, current ) );
	return true;
}

static bool testLayoutContracts( void ) noexcept
{
	sirHiZLayout_t layout = {};
	REQUIRE( sirHiZCalculateLayout( 0, 1, &layout ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZCalculateLayout( 1, 0, &layout ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZCalculateLayout( 1, 1, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZCalculateLayout( 1, 1, &layout ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( layout.nMipCount == 1 && layout.nTexelCount == 1 && layout.szMips[0].nOffset == 0 );
	REQUIRE( sirHiZCalculateLayout( 5, 3, &layout ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( layout.nMipCount == 4 && layout.nTexelCount == 24 );
	REQUIRE( layout.szMips[1].nWidth == 3 && layout.szMips[1].nHeight == 2 );
	REQUIRE( layout.szMips[2].nWidth == 2 && layout.szMips[2].nHeight == 1 );
	REQUIRE( layout.szMips[3].nWidth == 1 && layout.szMips[3].nHeight == 1 );
	REQUIRE( sirHiZCalculateLayout( UINT32_MAX, UINT32_MAX, &layout ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	REQUIRE( sirHiZCalculateLayout( UINT32_MAX, 1, &layout ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	return true;
}

static bool testBuildContracts( void ) noexcept
{
	sirHiZLayout_t layout = {};
	REQUIRE( sirHiZCalculateLayout( 3, 3, &layout ) == SIR_GRAPHICS_SUCCESS );
	const float szDepth[] = { 0.9f, 0.7f, 0.0f, 0.8f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
	float szOutput[32] = {};
	sirHiZSource_t source = { szDepth, 9, 3 };
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( layout.nTexelCount == 14 && szOutput[9] == 0.6f && szOutput[10] == 0.0f );
	REQUIRE( szOutput[11] == 0.0f && szOutput[12] == 0.0f && szOutput[13] == 0.0f );
	REQUIRE( sirHiZBuildReference( layout, source, nullptr, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 13 ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	source.nTexelCapacity = 8;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	source.nTexelCapacity = 9;
	source.nRowStride = 2;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	source.nRowStride = 3;
	source.pDepth = nullptr;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	source.pDepth = szOutput;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	source.pDepth = szDepth;
	layout.szMips[1].nOffset++;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.szMips[1].nOffset--;
	layout.nTexelCount++;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nTexelCount--;
	layout.szMips[1].nWidth++;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.szMips[1].nWidth--;
	layout.szMips[1].nHeight++;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.szMips[1].nHeight--;
	layout.nMipCount++;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nMipCount--;
	layout.nWidth = 0;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nWidth = 3;
	const float szInvalid[] = { 0.9f, 0.7f, -0.1f, 0.8f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
	source.pDepth = szInvalid;
	szOutput[0] = -5.0f;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT && szOutput[0] == -5.0f );
	const float szInvalidHigh[] = { 0.9f, 0.7f, 1.1f, 0.8f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f };
	source.pDepth = szInvalidHigh;
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	const float szPadding[] = { 0.9f, 0.7f, 0.0f, -1.0f, 0.8f, 0.6f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f };
	source = { szPadding, 12, 4 };
	REQUIRE( sirHiZBuildReference( layout, source, szOutput, 32 ) == SIR_GRAPHICS_SUCCESS );
	return true;
}

static bool testOcclusionContracts( void ) noexcept
{
	sirHiZLayout_t layout = {};
	REQUIRE( sirHiZCalculateLayout( 4, 4, &layout ) == SIR_GRAPHICS_SUCCESS );
	const float szDepth[] = { 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.1f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f };
	float szPyramid[32] = {};
	const sirHiZSource_t source = { szDepth, 16, 4 };
	REQUIRE( sirHiZBuildReference( layout, source, szPyramid, 32 ) == SIR_GRAPHICS_SUCCESS );
	sirHiZTest_t test = { 0, 0, 2, 2, 0.4f, 0.0f, false, 0 };
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && test.bOccluded );
	REQUIRE( test.nSelectedMip == 0 );
	test.nMaxX = 4;
	test.nMaxY = 4;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	REQUIRE( test.nSelectedMip > 0 );
	test = { 0, 0, 2, 2, 0.9f, 0.0f, true, 0 };
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	test = { 0, 0, 2, 2, 0.4f, 0.6f, true, 0 };
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	test.fBias = 0.0f;
	test.nMaxX = 5;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	test.nMaxX = 2;
	test.nMaxY = 5;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	test.nMaxX = 0;
	test.nMaxY = 2;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	test.nMaxX = 2;
	test.nMaxY = 0;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	test.nMaxY = 2;
	test.fBias = -0.1f;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	test.fBias = 1.1f;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	test.fBias = 0.0f;
	test.fNearestDepth = 1.1f;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	test.fNearestDepth = 0.4f;
	REQUIRE( sirHiZTestRectangle( layout, nullptr, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 1, &test ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nMipCount = 0;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nMipCount = 3;
	szPyramid[0] = -1.0f;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	szPyramid[0] = 1.1f;
	REQUIRE( sirHiZTestRectangle( layout, szPyramid, 32, &test ) == SIR_GRAPHICS_SUCCESS && !test.bOccluded );
	return true;
}

static bool testBatchCulling( void ) noexcept
{
	sirHiZLayout_t layout = {};
	REQUIRE( sirHiZCalculateLayout( 2, 2, &layout ) == SIR_GRAPHICS_SUCCESS );
	const float szDepth[] = { 0.9f, 0.9f, 0.9f, 0.1f };
	float szPyramid[8] = {};
	REQUIRE( sirHiZBuildReference( layout, { szDepth, 4, 2 }, szPyramid, 8 ) == SIR_GRAPHICS_SUCCESS );
	sirVisibilityCandidate_t szCandidates[3] = {};
	szCandidates[0] = { 14, { 0, 0, 1, 1, 0.5f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 18, { 1, 1, 2, 2, 0.5f, 0.0f, false, 0 }, false };
	szCandidates[2] = { 27, { 0, 0, 1, 1, 0.5f, 0.0f, false, 0 }, true };
	sirVisibilityCullDesc_t desc = { &layout, szPyramid, 8, szCandidates, 3, true };
	uint32_t szVisible[3] = { 0, 0, 0 };
	uint32_t szRejected[3] = { 0, 0, 0 };
	sirVisibilityOutput_t output = { szVisible, 3, 99, szRejected, 3, 99 };
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( output.nVisibleCount == 2 && output.nRejectedCount == 1 );
	REQUIRE( szVisible[0] == 18 && szVisible[1] == 27 && szRejected[0] == 14 );
	output.nVisibleCount = 99;
	output.nVisibleCapacity = 1;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_CAPACITY_EXCEEDED && output.nVisibleCount == 99 );
	output.nVisibleCapacity = 3;
	output.nRejectedCapacity = 0;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_CAPACITY_EXCEEDED );
	output.nRejectedCapacity = 3;
	desc.bHistoryValid = false;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( output.nVisibleCount == 3 && output.nRejectedCount == 0 );
	desc.bHistoryValid = true;
	desc.pPyramid = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pPyramid = szPyramid;
	desc.nPyramidTexels = 1;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nPyramidTexels = 8;
	desc.pCandidates = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pCandidates = szCandidates;
	szCandidates[0].bounds.fNearestDepth = -1.0f;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	szCandidates[0].bounds.fNearestDepth = 0.5f;
	output.pVisibleIds = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pVisibleIds = szVisible;
	output.pRejectedIds = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pRejectedIds = szRejected;
	REQUIRE( sirVisibilityCullReference( desc, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pLayout = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.pLayout = &layout;
	layout.nMipCount = 0;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	layout.nMipCount = 2;
	desc.nCandidateCount = 0;
	desc.pCandidates = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( output.nVisibleCount == 0 && output.nRejectedCount == 0 );
	output.nVisibleCapacity = 0;
	output.nRejectedCapacity = 0;
	output.pVisibleIds = nullptr;
	output.pRejectedIds = nullptr;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	return true;
}

static uint32_t nextRandom( uint32_t *pState ) noexcept
{
	*pState = *pState * 1664525u + 1013904223u;
	return *pState;
}

static bool testRandomDifferential( void ) noexcept
{
	uint32_t nSeed = 0x12345678u;
	for ( uint32_t iCase = 0; iCase < 200; ++iCase ) {
		const uint32_t nWidth = 1 + nextRandom( &nSeed ) % 37;
		const uint32_t nHeight = 1 + nextRandom( &nSeed ) % 29;
		sirHiZLayout_t layout = {};
		REQUIRE( sirHiZCalculateLayout( nWidth, nHeight, &layout ) == SIR_GRAPHICS_SUCCESS );
		std::vector<float> szDepth( ( size_t )nWidth * nHeight );
		for ( float& fValue : szDepth ) {
			fValue = ( float )( nextRandom( &nSeed ) % 1000 ) / 1000.0f;
		}
		std::vector<float> szPyramid( ( size_t )layout.nTexelCount );
		sirHiZSource_t source = { szDepth.data(), szDepth.size(), nWidth };
		REQUIRE( sirHiZBuildReference( layout, source, szPyramid.data(), szPyramid.size() ) == SIR_GRAPHICS_SUCCESS );
		for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
			const sirHiZMip_t& mip = layout.szMips[iMip];
			const uint32_t nScale = 1U << iMip;
			for ( uint32_t nY = 0; nY < mip.nHeight; ++nY ) {
				for ( uint32_t nX = 0; nX < mip.nWidth; ++nX ) {
					float fReference = 1.0f;
					for ( uint64_t nSourceY = ( uint64_t )nY * nScale; nSourceY < nHeight && nSourceY < ( uint64_t )( nY + 1 ) * nScale;
					      ++nSourceY ) {
						for ( uint64_t nSourceX = ( uint64_t )nX * nScale; nSourceX < nWidth && nSourceX < ( uint64_t )( nX + 1 ) * nScale;
						      ++nSourceX ) {
							const float fSample = szDepth[nSourceY * nWidth + nSourceX];
							if ( fSample < fReference ) {
								fReference = fSample;
							}
						}
					}
					REQUIRE( szPyramid[mip.nOffset + ( uint64_t )nY * mip.nWidth + nX] == fReference );
				}
			}
		}
		for ( uint32_t iQuery = 0; iQuery < 30; ++iQuery ) {
			const uint32_t nX = nextRandom( &nSeed ) % nWidth;
			const uint32_t nY = nextRandom( &nSeed ) % nHeight;
			sirHiZTest_t query = {
			    nX,    nY, 1 + nX + nextRandom( &nSeed ) % ( nWidth - nX ), 1 + nY + nextRandom( &nSeed ) % ( nHeight - nY ), 0.45f, 0.002f,
			    false, 0 };
			REQUIRE( sirHiZTestRectangle( layout, szPyramid.data(), szPyramid.size(), &query ) == SIR_GRAPHICS_SUCCESS );
			if ( query.bOccluded ) {
				for ( uint32_t nCheckY = query.nMinY; nCheckY < query.nMaxY; ++nCheckY ) {
					for ( uint32_t nCheckX = query.nMinX; nCheckX < query.nMaxX; ++nCheckX ) {
						REQUIRE( query.fNearestDepth + query.fBias < szDepth[( uint64_t )nCheckY * nWidth + nCheckX] );
					}
				}
			}
		}
	}
	return true;
}

// Offset aliases must fail before the pyramid or compacted IDs are modified.
static bool testAliasingContracts( void ) noexcept
{
	sirHiZLayout_t layout = {};
	REQUIRE( sirHiZCalculateLayout( 2, 2, &layout ) == SIR_GRAPHICS_SUCCESS );
	float szStorage[32] = {};
	szStorage[0] = 0.9f;
	szStorage[1] = 0.1f;
	szStorage[2] = 0.9f;
	szStorage[3] = 0.9f;
	sirHiZSource_t source = { szStorage, 4, 2 };
	REQUIRE( sirHiZBuildReference( layout, source, szStorage + 1, 31 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( szStorage[0] == 0.9f && szStorage[1] == 0.1f );
	source.pDepth = szStorage + 1;
	REQUIRE( sirHiZBuildReference( layout, source, szStorage, 32 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	source.pDepth = szStorage;
	REQUIRE( sirHiZBuildReference( layout, source, szStorage + 8, 24 ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( szStorage[8] == 0.9f && szStorage[9] == 0.1f );

	sirVisibilityCandidate_t szCandidates[2] = {};
	szCandidates[0] = { 41, { 0, 0, 1, 1, 0.5f, 0.0f, false, 0 }, false };
	szCandidates[1] = { 42, { 1, 0, 2, 1, 0.5f, 0.0f, false, 0 }, false };
	sirVisibilityCullDesc_t desc = { &layout, szStorage + 8, layout.nTexelCount, szCandidates, 2, true };
	uint32_t szVisible[2] = { 123, 456 };
	uint32_t szRejected[2] = { 789, 101 };
	sirVisibilityOutput_t output = { szVisible, 2, 77, szRejected, 2, 88 };
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( output.nVisibleCount == 1 && output.nRejectedCount == 1 );
	REQUIRE( szVisible[0] == 42 && szRejected[0] == 41 );

	output.nVisibleCount = 77;
	output.nRejectedCount = 88;
	output.pVisibleIds = &szCandidates[0].nInstanceId;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( szCandidates[0].nInstanceId == 41 && output.nVisibleCount == 77 );
	output.pVisibleIds = ( uint32_t * )( szStorage + 9 );
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( szStorage[9] == 0.1f && output.nVisibleCount == 77 );
	output.pVisibleIds = szVisible;
	output.pRejectedIds = szVisible;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( szVisible[0] == 42 && output.nRejectedCount == 88 );
	output.pRejectedIds = &szCandidates[0].nInstanceId;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pRejectedIds = ( uint32_t * )( szStorage + 8 );
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pRejectedIds = szRejected;
	output.pVisibleIds = &output.nVisibleCount;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pVisibleIds = szVisible;
	output.pRejectedIds = &output.nRejectedCount;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	output.pRejectedIds = szRejected;
	REQUIRE( sirVisibilityCullReference( desc, &output ) == SIR_GRAPHICS_SUCCESS );
	return true;
}

int main( void )
{
	if ( !testMethodContracts() || !testHistoryContracts() || !testLayoutContracts() || !testBuildContracts() ||
	     !testOcclusionContracts() || !testBatchCulling() || !testRandomDifferential() || !testAliasingContracts() ) {
		return 1;
	}
	puts( "PASS: HIZ-00 contracts, negative paths, odd mip extents, and randomized conservative visibility" );
	return 0;
}

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

sirGraphicsResult_t sirVisibilityValidateMethod( const sirVisibilityMethodDesc_t& method,
                                                 const sirGraphicsCapabilities_t& capabilities ) noexcept
{
	if ( method.eMode > SIR_VISIBILITY_AUTO || method.eDepthConvention > SIR_VISIBILITY_FORWARD_Z ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( method.eDepthFormat != SIR_GRAPHICS_FORMAT_D16_UNORM && method.eDepthFormat != SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT &&
	     method.eDepthFormat != SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( method.eMode == SIR_VISIBILITY_AUTO ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( method.eMode == SIR_VISIBILITY_DISABLED ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( method.nMaximumCandidates == 0 || method.nMaximumDraws == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( method.eDepthConvention != SIR_VISIBILITY_REVERSE_Z ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	constexpr uint64_t REQUIRED = SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE | SIR_GRAPHICS_FEATURE_INDIRECT_DRAW;
	if ( ( capabilities.nFeatureMask & REQUIRED ) != REQUIRED ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

bool sirVisibilityCanReuseHistory( const sirVisibilityView_t& previous, const sirVisibilityView_t& current ) noexcept
{
	if ( previous.nViewId == 0 || current.bCameraCut || previous.bCameraCut ) {
		return false;
	}
	if ( previous.nFrameIndex == UINT64_MAX || current.nFrameIndex != previous.nFrameIndex + 1 ) {
		return false;
	}
	if ( previous.nViewId != current.nViewId || previous.nProjectionRevision != current.nProjectionRevision ||
	     previous.nWorldOriginRevision != current.nWorldOriginRevision ||
	     previous.nDepthProducerRevision != current.nDepthProducerRevision ||
	     previous.nSceneResetRevision != current.nSceneResetRevision ) {
		return false;
	}
	if ( previous.nDeviceGeneration == 0 || previous.nDeviceGeneration != current.nDeviceGeneration ) {
		return false;
	}
	if ( previous.nWidth == 0 || previous.nHeight == 0 || previous.nWidth != current.nWidth || previous.nHeight != current.nHeight ||
	     previous.nSampleCount == 0 || previous.nSampleCount != current.nSampleCount ) {
		return false;
	}
	return previous.eDepthFormat == current.eDepthFormat && previous.eDepthConvention == SIR_VISIBILITY_REVERSE_Z &&
	       previous.eDepthConvention == current.eDepthConvention;
}

sirGraphicsResult_t sirHiZCalculateLayout( uint32_t nWidth, uint32_t nHeight, sirHiZLayout_t *pLayout ) noexcept
{
	if ( pLayout == nullptr || nWidth == 0 || nHeight == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirHiZLayout_t result = {};
	result.nWidth = nWidth;
	result.nHeight = nHeight;
	uint32_t nLevelWidth = nWidth;
	uint32_t nLevelHeight = nHeight;
	for ( uint32_t iMip = 0; iMip < 32; ++iMip ) {
		const uint64_t nTexels = ( uint64_t )nLevelWidth * nLevelHeight;
		if ( UINT64_MAX - result.nTexelCount < nTexels ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		result.szMips[iMip] = { result.nTexelCount, nLevelWidth, nLevelHeight };
		result.nTexelCount += nTexels;
		++result.nMipCount;
		if ( nLevelWidth == 1 && nLevelHeight == 1 ) {
			*pLayout = result;
			return SIR_GRAPHICS_SUCCESS;
		}
		nLevelWidth = nLevelWidth / 2 + nLevelWidth % 2;
		nLevelHeight = nLevelHeight / 2 + nLevelHeight % 2;
	}
	return SIR_GRAPHICS_CAPACITY_EXCEEDED;
}

// Interpret only the addressed byte ranges. This avoids pointer arithmetic
// across unrelated allocations and catches offset aliases missed by equality.
static bool sirVisibilityRangesOverlap( const void *pFirst, uint64_t nFirstBytes, const void *pSecond, uint64_t nSecondBytes ) noexcept
{
	if ( pFirst == nullptr || pSecond == nullptr || nFirstBytes == 0 || nSecondBytes == 0 ) {
		return false;
	}
	const uintptr_t nFirst = ( uintptr_t )pFirst;
	const uintptr_t nSecond = ( uintptr_t )pSecond;
	if ( nFirst <= nSecond ) {
		return ( uint64_t )( nSecond - nFirst ) < nFirstBytes;
	}
	return ( uint64_t )( nFirst - nSecond ) < nSecondBytes;
}

static bool sirHiZLayoutMatches( const sirHiZLayout_t& layout ) noexcept
{
	sirHiZLayout_t expected = {};
	if ( sirHiZCalculateLayout( layout.nWidth, layout.nHeight, &expected ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( layout.nMipCount != expected.nMipCount || layout.nTexelCount != expected.nTexelCount ) {
		return false;
	}
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		const sirHiZMip_t& actual = layout.szMips[iMip];
		const sirHiZMip_t& reference = expected.szMips[iMip];
		if ( actual.nOffset != reference.nOffset || actual.nWidth != reference.nWidth || actual.nHeight != reference.nHeight ) {
			return false;
		}
	}
	return true;
}

sirGraphicsResult_t sirHiZBuildReference( const sirHiZLayout_t& layout, const sirHiZSource_t& source, float *pOutput,
                                          uint64_t nOutputTexels ) noexcept
{
	if ( !sirHiZLayoutMatches( layout ) || source.pDepth == nullptr || pOutput == nullptr || source.pDepth == pOutput ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( source.nRowStride < layout.nWidth || nOutputTexels < layout.nTexelCount ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const uint64_t nRequiredInput = ( uint64_t )( layout.nHeight - 1 ) * source.nRowStride + layout.nWidth;
	if ( source.nTexelCapacity < nRequiredInput || nRequiredInput > ( uint64_t )SIZE_MAX / sizeof( float ) ||
	     layout.nTexelCount > ( uint64_t )SIZE_MAX / sizeof( float ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( sirVisibilityRangesOverlap( source.pDepth, nRequiredInput * sizeof( float ), pOutput,
	                                 layout.nTexelCount * sizeof( float ) ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t nY = 0; nY < layout.nHeight; ++nY ) {
		for ( uint32_t nX = 0; nX < layout.nWidth; ++nX ) {
			const float fDepth = source.pDepth[( uint64_t )nY * source.nRowStride + nX];
			if ( !( fDepth >= 0.0f && fDepth <= 1.0f ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}

	for ( uint32_t nY = 0; nY < layout.nHeight; ++nY ) {
		memcpy( pOutput + ( uint64_t )nY * layout.nWidth, source.pDepth + ( uint64_t )nY * source.nRowStride,
		        ( size_t )layout.nWidth * sizeof( float ) );
	}
	for ( uint32_t iMip = 1; iMip < layout.nMipCount; ++iMip ) {
		const sirHiZMip_t& previous = layout.szMips[iMip - 1];
		const sirHiZMip_t& current = layout.szMips[iMip];
		for ( uint32_t nY = 0; nY < current.nHeight; ++nY ) {
			for ( uint32_t nX = 0; nX < current.nWidth; ++nX ) {
				float fMinimum = 1.0f;
				for ( uint32_t iY = 0; iY < 2; ++iY ) {
					const uint64_t nSourceY = ( uint64_t )nY * 2 + iY;
					if ( nSourceY >= previous.nHeight ) {
						continue;
					}
					for ( uint32_t iX = 0; iX < 2; ++iX ) {
						const uint64_t nSourceX = ( uint64_t )nX * 2 + iX;
						if ( nSourceX >= previous.nWidth ) {
							continue;
						}
						const float fValue = pOutput[previous.nOffset + nSourceY * previous.nWidth + nSourceX];
						if ( fValue < fMinimum ) {
							fMinimum = fValue;
						}
					}
				}
				pOutput[current.nOffset + ( uint64_t )nY * current.nWidth + nX] = fMinimum;
			}
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZTestRectangle( const sirHiZLayout_t& layout, const float *pPyramid, uint64_t nPyramidTexels,
                                         sirHiZTest_t *pTest ) noexcept
{
	if ( pTest == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pTest->bOccluded = false;
	pTest->nSelectedMip = 0;
	if ( !sirHiZLayoutMatches( layout ) || pPyramid == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nPyramidTexels < layout.nTexelCount ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	if ( pTest->nMinX >= pTest->nMaxX || pTest->nMinY >= pTest->nMaxY || !( pTest->fBias >= 0.0f && pTest->fBias <= 1.0f ) ||
	     !( pTest->fNearestDepth >= 0.0f && pTest->fNearestDepth <= 1.0f ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTest->nMaxX > layout.nWidth || pTest->nMaxY > layout.nHeight ) {
		return SIR_GRAPHICS_SUCCESS;
	}

	uint32_t iMip = 0;
	for ( ; iMip + 1 < layout.nMipCount; ++iMip ) {
		const uint32_t nTilesX = ( ( pTest->nMaxX - 1 ) >> iMip ) - ( pTest->nMinX >> iMip ) + 1;
		const uint32_t nTilesY = ( ( pTest->nMaxY - 1 ) >> iMip ) - ( pTest->nMinY >> iMip ) + 1;
		if ( nTilesX <= 2 && nTilesY <= 2 ) {
			break;
		}
	}
	pTest->nSelectedMip = iMip;
	const sirHiZMip_t& mip = layout.szMips[iMip];
	const uint32_t nX0 = pTest->nMinX >> iMip;
	const uint32_t nY0 = pTest->nMinY >> iMip;
	const uint32_t nX1 = ( pTest->nMaxX - 1 ) >> iMip;
	const uint32_t nY1 = ( pTest->nMaxY - 1 ) >> iMip;
	for ( uint32_t nY = nY0; nY <= nY1; ++nY ) {
		for ( uint32_t nX = nX0; nX <= nX1; ++nX ) {
			const float fOccluder = pPyramid[mip.nOffset + ( uint64_t )nY * mip.nWidth + nX];
			if ( !( fOccluder >= 0.0f && fOccluder <= 1.0f ) ) {
				return SIR_GRAPHICS_SUCCESS;
			}
			if ( !( pTest->fNearestDepth + pTest->fBias < fOccluder ) ) {
				return SIR_GRAPHICS_SUCCESS;
			}
		}
	}
	pTest->bOccluded = true;
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirVisibilityClassifyCandidate( const sirVisibilityCullDesc_t& desc, const sirVisibilityCandidate_t& candidate,
                                                           bool *pVisible ) noexcept
{
	*pVisible = true;
	if ( candidate.bForceVisible || !desc.bHistoryValid ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	sirHiZTest_t query = candidate.bounds;
	const sirGraphicsResult_t eResult = sirHiZTestRectangle( *desc.pLayout, desc.pPyramid, desc.nPyramidTexels, &query );
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		*pVisible = !query.bOccluded;
	}
	return eResult;
}

sirGraphicsResult_t sirVisibilityCullReference( const sirVisibilityCullDesc_t& desc, sirVisibilityOutput_t *pOutput ) noexcept
{
	if ( pOutput == nullptr || desc.pLayout == nullptr || !sirHiZLayoutMatches( *desc.pLayout ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nCandidateCount != 0 && desc.pCandidates == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.bHistoryValid && ( desc.pPyramid == nullptr || desc.nPyramidTexels < desc.pLayout->nTexelCount ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pOutput->nVisibleCapacity != 0 && pOutput->pVisibleIds == nullptr ) ||
	     ( pOutput->nRejectedCapacity != 0 && pOutput->pRejectedIds == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.pLayout->nTexelCount > ( uint64_t )SIZE_MAX / sizeof( float ) ||
	     desc.nCandidateCount > ( uint64_t )SIZE_MAX / sizeof( sirVisibilityCandidate_t ) ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	const uint64_t nCandidateBytes = ( uint64_t )desc.nCandidateCount * sizeof( sirVisibilityCandidate_t );
	const uint64_t nPyramidBytes = desc.bHistoryValid ? desc.pLayout->nTexelCount * sizeof( float ) : 0;
	if ( sirVisibilityRangesOverlap( pOutput, sizeof( *pOutput ), &desc, sizeof( desc ) ) ||
	     sirVisibilityRangesOverlap( pOutput, sizeof( *pOutput ), desc.pLayout, sizeof( *desc.pLayout ) ) ||
	     sirVisibilityRangesOverlap( pOutput, sizeof( *pOutput ), desc.pCandidates, nCandidateBytes ) ||
	     sirVisibilityRangesOverlap( pOutput, sizeof( *pOutput ), desc.pPyramid, nPyramidBytes ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	uint32_t nVisibleCount = 0;
	uint32_t nRejectedCount = 0;
	for ( uint32_t iCandidate = 0; iCandidate < desc.nCandidateCount; ++iCandidate ) {
		bool bVisible = true;
		const sirGraphicsResult_t eResult = sirVisibilityClassifyCandidate( desc, desc.pCandidates[iCandidate], &bVisible );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		if ( bVisible ) {
			++nVisibleCount;
		} else {
			++nRejectedCount;
		}
	}
	if ( nVisibleCount > pOutput->nVisibleCapacity || nRejectedCount > pOutput->nRejectedCapacity ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	// The two classification passes must observe the same immutable inputs.
	// Reject aliases before either ID buffer or output count is published.
	const uint64_t nVisibleBytes = ( uint64_t )nVisibleCount * sizeof( uint32_t );
	const uint64_t nRejectedBytes = ( uint64_t )nRejectedCount * sizeof( uint32_t );
	if ( sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, pOutput->pRejectedIds, nRejectedBytes ) ||
	     sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, pOutput, sizeof( *pOutput ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pRejectedIds, nRejectedBytes, pOutput, sizeof( *pOutput ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, &desc, sizeof( desc ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pRejectedIds, nRejectedBytes, &desc, sizeof( desc ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, desc.pLayout, sizeof( *desc.pLayout ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pRejectedIds, nRejectedBytes, desc.pLayout, sizeof( *desc.pLayout ) ) ||
	     sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, desc.pCandidates, nCandidateBytes ) ||
	     sirVisibilityRangesOverlap( pOutput->pRejectedIds, nRejectedBytes, desc.pCandidates, nCandidateBytes ) ||
	     sirVisibilityRangesOverlap( pOutput->pVisibleIds, nVisibleBytes, desc.pPyramid, nPyramidBytes ) ||
	     sirVisibilityRangesOverlap( pOutput->pRejectedIds, nRejectedBytes, desc.pPyramid, nPyramidBytes ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	nVisibleCount = 0;
	nRejectedCount = 0;
	for ( uint32_t iCandidate = 0; iCandidate < desc.nCandidateCount; ++iCandidate ) {
		bool bVisible = true;
		( void )sirVisibilityClassifyCandidate( desc, desc.pCandidates[iCandidate], &bVisible );
		if ( bVisible ) {
			pOutput->pVisibleIds[nVisibleCount++] = desc.pCandidates[iCandidate].nInstanceId;
		} else {
			pOutput->pRejectedIds[nRejectedCount++] = desc.pCandidates[iCandidate].nInstanceId;
		}
	}
	pOutput->nVisibleCount = nVisibleCount;
	pOutput->nRejectedCount = nRejectedCount;
	return SIR_GRAPHICS_SUCCESS;
}

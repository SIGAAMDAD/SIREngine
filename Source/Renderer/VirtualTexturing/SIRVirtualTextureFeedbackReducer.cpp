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

#include <string.h>

// Port of Terrain3D runtime/terrain_3d_vt_feedback_reducer.h.
static uint32_t sirVTFeedbackMin( uint32_t nLeft, uint32_t nRight ) noexcept
{
	return nLeft < nRight ? nLeft : nRight;
}

static uint32_t sirVTFeedbackAdd( uint32_t nLeft, uint32_t nRight ) noexcept
{
	return UINT32_MAX - nLeft < nRight ? UINT32_MAX : nLeft + nRight;
}

static bool sirVTFeedbackWorse( const sirVirtualTextureFeedbackCandidate_t& left,
                                const sirVirtualTextureFeedbackCandidate_t& right ) noexcept
{
	return left.nScore < right.nScore || ( left.nScore == right.nScore && left.nPage > right.nPage );
}

bool SIRVirtualTextureFeedbackReducer::bindCold( const sirVirtualTextureFeedbackReducerBuffers_t& buffers, uint32_t nPageCapacity,
                                                 uint32_t nCandidateCapacity ) noexcept
{
	resetCold();
	if ( nPageCapacity == 0 || nCandidateCapacity == 0 || buffers.pEpoch == nullptr || buffers.pDirect == nullptr ||
	     buffers.pAggregate == nullptr || buffers.pTouched == nullptr || buffers.pCandidates == nullptr ) {
		return false;
	}
	m_Buffers = buffers;
	m_nPageCapacity = nPageCapacity;
	m_nCandidateCapacity = nCandidateCapacity;
	memset( m_Buffers.pEpoch, 0, ( size_t )nPageCapacity * sizeof( uint32_t ) );
	return true;
}

void SIRVirtualTextureFeedbackReducer::resetCold( void ) noexcept
{
	m_Buffers = {};
	m_nPageCapacity = 0;
	m_nCandidateCapacity = 0;
	m_nEpoch = 0;
	m_nTouched = 0;
	m_nSelected = 0;
}

void SIRVirtualTextureFeedbackReducer::discardCapture( void ) noexcept
{
	m_nTouched = 0;
	m_nSelected = 0;
}

uint32_t SIRVirtualTextureFeedbackReducer::getTouchedCount( void ) const noexcept
{
	return m_nTouched;
}

uint32_t SIRVirtualTextureFeedbackReducer::getTouchedPage( uint32_t nIndex ) const noexcept
{
	return nIndex < m_nTouched ? m_Buffers.pTouched[nIndex] : UINT32_MAX;
}

uint32_t SIRVirtualTextureFeedbackReducer::getSelectedCount( void ) const noexcept
{
	return m_nSelected;
}

const sirVirtualTextureFeedbackCandidate_t *SIRVirtualTextureFeedbackReducer::getSelected( uint32_t nIndex ) const noexcept
{
	return nIndex < m_nSelected ? &m_Buffers.pCandidates[nIndex] : nullptr;
}

bool SIRVirtualTextureFeedbackReducer::hasDirectPage( uint32_t nPage ) const noexcept
{
	return m_nTouched != 0 && m_nEpoch != 0 && m_Buffers.pEpoch != nullptr && m_Buffers.pDirect != nullptr && nPage < m_nPageCapacity &&
	       m_Buffers.pEpoch[nPage] == m_nEpoch && m_Buffers.pDirect[nPage] != 0;
}

bool SIRVirtualTextureFeedbackReducer::reduce( const sirVirtualTextureFeedbackReduceInput_t& input,
                                               const sirVirtualTextureFeedbackPolicy_t& policy,
                                               sirVirtualTextureFeedbackReductionStats_t *pStats ) noexcept
{
	return reduceInternal( input, policy, pStats, false );
}

bool SIRVirtualTextureFeedbackReducer::reduceMulti( const sirVirtualTextureFeedbackReduceInput_t& input,
                                                    const sirVirtualTextureFeedbackPolicy_t& policy,
                                                    sirVirtualTextureFeedbackReductionStats_t *pStats ) noexcept
{
	if ( pStats == nullptr ) {
		discardCapture();
		return false;
	}
	*pStats = {};
	discardCapture();
	if ( input.pBytes == nullptr || input.nRecords > 240U * 136U ||
	     input.nAvailableBytes < ( uint64_t )input.nRecords * sizeof( sirVirtualTextureFeedbackMultiRecord_t ) ) {
		return false;
	}
	for ( uint32_t iCell = 0; iCell < input.nRecords; ++iCell ) {
		const uint8_t *pCell = input.pBytes + ( size_t )iCell * sizeof( sirVirtualTextureFeedbackMultiRecord_t );
		uint32_t nOverflow = 0;
		memcpy( &nOverflow, pCell + 8U * sizeof( uint32_t ), sizeof( nOverflow ) );
		if ( nOverflow != 0 ) {
			return false;
		}
		uint32_t nCountSum = 0;
		for ( uint32_t j = 0; j < SIR_VT_FEEDBACK_MULTI_PAGES; ++j ) {
			uint32_t nPage = 0;
			uint32_t nWeight = 0;
			memcpy( &nPage, pCell + j * sizeof( uint32_t ), sizeof( nPage ) );
			memcpy( &nWeight, pCell + ( SIR_VT_FEEDBACK_MULTI_PAGES + j ) * sizeof( uint32_t ), sizeof( nWeight ) );
			if ( nPage == SIR_VT_FEEDBACK_INVALID_PAGE ) {
				if ( nWeight != 0 ) {
					return false;
				}
				continue;
			}
			if ( nPage >= input.nPageCount || nWeight == 0 ||
			     nWeight > SIR_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL * SIR_VT_FEEDBACK_MULTI_MAX_LAYERS ||
			     UINT32_MAX - nCountSum < nWeight ) {
				return false;
			}
			nCountSum += nWeight;
			for ( uint32_t k = 0; k < j; ++k ) {
				uint32_t nOther = 0;
				memcpy( &nOther, pCell + k * sizeof( uint32_t ), sizeof( nOther ) );
				if ( nOther == nPage ) {
					return false;
				}
			}
		}
		if ( nCountSum > SIR_VT_FEEDBACK_MULTI_MAX_PIXELS_PER_CELL * SIR_VT_FEEDBACK_MULTI_MAX_LAYERS ) {
			return false;
		}
	}
	return reduceInternal( input, policy, pStats, true );
}

bool SIRVirtualTextureFeedbackReducer::reduceInternal( const sirVirtualTextureFeedbackReduceInput_t& input,
                                                       const sirVirtualTextureFeedbackPolicy_t& policy,
                                                       sirVirtualTextureFeedbackReductionStats_t *pStats, bool bMulti ) noexcept
{
	if ( pStats == nullptr ) {
		discardCapture();
		return false;
	}
	*pStats = {};
	discardCapture();
	const uint64_t nStride = bMulti ? sizeof( sirVirtualTextureFeedbackMultiRecord_t ) : sizeof( uint32_t );
	if ( input.pBytes == nullptr || input.pfnLookup == nullptr || m_Buffers.pEpoch == nullptr || input.nPageCount > m_nPageCapacity ||
	     m_nCandidateCapacity == 0 || input.nAvailableBytes < ( uint64_t )input.nRecords * nStride ) {
		return false;
	}
	if ( ++m_nEpoch == 0 ) {
		memset( m_Buffers.pEpoch, 0, ( size_t )m_nPageCapacity * sizeof( uint32_t ) );
		m_nEpoch = 1;
	}
	pStats->nSamples = bMulti ? 0 : input.nRecords;
	for ( uint32_t i = 0; i < input.nRecords; ++i ) {
		const uint32_t nSamples = bMulti ? SIR_VT_FEEDBACK_MULTI_PAGES : 1U;
		const uint8_t *pWord = input.pBytes + ( size_t )i * nStride;
		for ( uint32_t j = 0; j < nSamples; ++j ) {
			uint32_t nPage = 0;
			uint32_t nWeight = 1;
			memcpy( &nPage, pWord + ( size_t )j * sizeof( uint32_t ), sizeof( nPage ) );
			if ( bMulti ) {
				memcpy( &nWeight, pWord + ( size_t )( SIR_VT_FEEDBACK_MULTI_PAGES + j ) * sizeof( uint32_t ), sizeof( nWeight ) );
				pStats->nSamples = sirVTFeedbackAdd( pStats->nSamples, nWeight );
			}
			if ( nPage >= input.nPageCount || nWeight == 0 ) {
				continue;
			}
			pStats->nValidSamples = sirVTFeedbackAdd( pStats->nValidSamples, nWeight );
			visit( nPage );
			m_Buffers.pDirect[nPage] = sirVTFeedbackAdd( m_Buffers.pDirect[nPage], nWeight );
			m_Buffers.pAggregate[nPage] = sirVTFeedbackAdd( m_Buffers.pAggregate[nPage], nWeight );
		}
	}
	const uint32_t nLeaves = m_nTouched;
	pStats->nDirectUniquePages = nLeaves;
	for ( uint32_t i = 0; i < nLeaves; ++i ) {
		const uint32_t nLeaf = m_Buffers.pTouched[i];
		const uint32_t nCount = m_Buffers.pDirect[nLeaf];
		uint32_t nPage = nLeaf;
		uint32_t szAncestors[SIR_VT_FEEDBACK_MAX_PARENT_DEPTH + 1] = { nLeaf };
		for ( uint32_t nDepth = 0; nDepth < SIR_VT_FEEDBACK_MAX_PARENT_DEPTH; ++nDepth ) {
			uint32_t nParent = UINT32_MAX;
			uint16_t nMip = 0;
			if ( !input.pfnLookup( input.pUser, nPage, &nParent, &nMip ) ) {
				++pStats->nInvalidParents;
				break;
			}
			if ( nParent == UINT32_MAX ) {
				break;
			}
			if ( nParent >= input.nPageCount || nParent == nPage ) {
				++pStats->nInvalidParents;
				break;
			}
			bool bCycle = false;
			for ( uint32_t j = 0; j <= nDepth; ++j ) {
				if ( szAncestors[j] == nParent ) {
					bCycle = true;
					break;
				}
			}
			if ( bCycle || nDepth + 1U == SIR_VT_FEEDBACK_MAX_PARENT_DEPTH ) {
				++pStats->nInvalidParents;
				break;
			}
			szAncestors[nDepth + 1U] = nParent;
			visit( nParent );
			m_Buffers.pAggregate[nParent] = sirVTFeedbackAdd( m_Buffers.pAggregate[nParent], nCount );
			++pStats->nParentPromotions;
			nPage = nParent;
		}
	}
	pStats->nUniquePages = m_nTouched;
	const uint32_t nLimit = sirVTFeedbackMin( policy.nMaxCandidates, m_nCandidateCapacity );
	for ( uint32_t i = 0; i < m_nTouched; ++i ) {
		const uint32_t nPage = m_Buffers.pTouched[i];
		uint32_t nParent = UINT32_MAX;
		uint16_t nMip = 0;
		if ( !input.pfnLookup( input.pUser, nPage, &nParent, &nMip ) ) {
			continue;
		}
		const uint32_t nCount = m_Buffers.pAggregate[nPage];
		const bool bRoot = nParent == UINT32_MAX;
		if ( nCount < policy.nMinimumSamples && !bRoot ) {
			++pStats->nRejectedNoise;
			continue;
		}
		const bool bPromoted = nCount > m_Buffers.pDirect[nPage];
		uint32_t nScore = sirVTFeedbackAdd( nCount > UINT32_MAX / 64U ? UINT32_MAX : nCount * 64U, bRoot       ? policy.nRootBoost
		                                                                                           : bPromoted ? policy.nParentBoost
		                                                                                                       : 0U );
		if ( policy.nMotionLevel != 0 ) {
			const uint32_t nMotion = sirVTFeedbackMin( policy.nMotionLevel, 2U );
			nScore = sirVTFeedbackAdd( nScore, ( bRoot ? 4096U : bPromoted ? 768U : 0U ) * nMotion );
		}
		const sirVirtualTextureFeedbackCandidate_t candidate = { nPage, nCount, nScore, nMip, 0 };
		if ( m_nSelected < nLimit ) {
			m_Buffers.pCandidates[m_nSelected] = candidate;
			siftUp( m_nSelected++ );
		} else if ( nLimit != 0 && sirVTFeedbackWorse( m_Buffers.pCandidates[0], candidate ) ) {
			m_Buffers.pCandidates[0] = candidate;
			siftDown( 0, m_nSelected );
		}
	}
	for ( uint32_t n = m_nSelected; n > 1; --n ) {
		swap( 0, n - 1U );
		siftDown( 0, n - 1U );
	}
	pStats->nCandidates = m_nSelected;
	return true;
}

void SIRVirtualTextureFeedbackReducer::visit( uint32_t nPage ) noexcept
{
	if ( m_Buffers.pEpoch[nPage] == m_nEpoch ) {
		return;
	}
	m_Buffers.pEpoch[nPage] = m_nEpoch;
	m_Buffers.pDirect[nPage] = 0;
	m_Buffers.pAggregate[nPage] = 0;
	m_Buffers.pTouched[m_nTouched++] = nPage;
}

void SIRVirtualTextureFeedbackReducer::swap( uint32_t nLeft, uint32_t nRight ) noexcept
{
	const sirVirtualTextureFeedbackCandidate_t candidate = m_Buffers.pCandidates[nLeft];
	m_Buffers.pCandidates[nLeft] = m_Buffers.pCandidates[nRight];
	m_Buffers.pCandidates[nRight] = candidate;
}

void SIRVirtualTextureFeedbackReducer::siftUp( uint32_t nIndex ) noexcept
{
	while ( nIndex != 0 ) {
		const uint32_t nParent = ( nIndex - 1U ) / 2U;
		if ( !sirVTFeedbackWorse( m_Buffers.pCandidates[nIndex], m_Buffers.pCandidates[nParent] ) ) {
			break;
		}
		swap( nIndex, nParent );
		nIndex = nParent;
	}
}

void SIRVirtualTextureFeedbackReducer::siftDown( uint32_t nIndex, uint32_t nCount ) noexcept
{
	while ( true ) {
		uint32_t nChild = 2U * nIndex + 1U;
		if ( nChild >= nCount ) {
			break;
		}
		if ( nChild + 1U < nCount && sirVTFeedbackWorse( m_Buffers.pCandidates[nChild + 1U], m_Buffers.pCandidates[nChild] ) ) {
			++nChild;
		}
		if ( !sirVTFeedbackWorse( m_Buffers.pCandidates[nChild], m_Buffers.pCandidates[nIndex] ) ) {
			break;
		}
		swap( nIndex, nChild );
		nIndex = nChild;
	}
}

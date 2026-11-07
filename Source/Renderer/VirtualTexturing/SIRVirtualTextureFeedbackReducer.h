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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACKREDUCER_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACKREDUCER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTextureFeedback.h>

static constexpr uint32_t SIR_VT_FEEDBACK_MAX_PARENT_DEPTH = 32U;

typedef struct sirVirtualTextureFeedbackCandidate_s
{
	uint32_t nPage;
	uint32_t nFrequency;
	uint32_t nScore;
	uint16_t nMip;
	uint16_t nReserved;
} sirVirtualTextureFeedbackCandidate_t;

typedef struct sirVirtualTextureFeedbackPolicy_s
{
	uint32_t nMinimumSamples = 2;
	uint32_t nMaxCandidates = 85;
	uint32_t nParentBoost = 512;
	uint32_t nRootBoost = 1536;
	uint32_t nMotionLevel = 0;
} sirVirtualTextureFeedbackPolicy_t;

typedef struct sirVirtualTextureFeedbackReductionStats_s
{
	uint32_t nSamples;
	uint32_t nValidSamples;
	uint32_t nUniquePages;
	uint32_t nDirectUniquePages;
	uint32_t nParentPromotions;
	uint32_t nRejectedNoise;
	uint32_t nInvalidParents;
	uint32_t nCandidates;
} sirVirtualTextureFeedbackReductionStats_t;

typedef struct sirVirtualTextureFeedbackReducerBuffers_s
{
	uint32_t *pEpoch;
	uint32_t *pDirect;
	uint32_t *pAggregate;
	uint32_t *pTouched;
	sirVirtualTextureFeedbackCandidate_t *pCandidates;
} sirVirtualTextureFeedbackReducerBuffers_t;

typedef bool ( *sirVirtualTextureFeedbackPageLookup_t )( void *pUser, uint32_t nPage, uint32_t *pParent, uint16_t *pMip ) noexcept;

typedef struct sirVirtualTextureFeedbackReduceInput_s
{
	const uint8_t *pBytes;
	uint64_t nAvailableBytes;
	uint32_t nRecords;
	uint32_t nPageCount;
	sirVirtualTextureFeedbackPageLookup_t pfnLookup;
	void *pUser;
} sirVirtualTextureFeedbackReduceInput_t;

// Terrain3D V2/V3 donor reducer: caller owns all capacity-sized cold arrays.
// A V3 malformed/overflow capture is rejected atomically and clears selections.
class SIRVirtualTextureFeedbackReducer
{
public:
	SIRVirtualTextureFeedbackReducer( void ) noexcept = default;
	~SIRVirtualTextureFeedbackReducer( void ) noexcept = default;

	uint32_t getTouchedCount( void ) const noexcept;
	uint32_t getTouchedPage( uint32_t nIndex ) const noexcept;
	uint32_t getSelectedCount( void ) const noexcept;
	const sirVirtualTextureFeedbackCandidate_t *getSelected( uint32_t nIndex ) const noexcept;
	bool hasDirectPage( uint32_t nPage ) const noexcept;

	bool bindCold( const sirVirtualTextureFeedbackReducerBuffers_t& buffers, uint32_t nPageCapacity, uint32_t nCandidateCapacity ) noexcept;
	void resetCold( void ) noexcept;
	void discardCapture( void ) noexcept;
	bool reduce( const sirVirtualTextureFeedbackReduceInput_t& input, const sirVirtualTextureFeedbackPolicy_t& policy,
	             sirVirtualTextureFeedbackReductionStats_t *pStats ) noexcept;
	bool reduceMulti( const sirVirtualTextureFeedbackReduceInput_t& input, const sirVirtualTextureFeedbackPolicy_t& policy,
	                  sirVirtualTextureFeedbackReductionStats_t *pStats ) noexcept;

protected:
private:
	bool reduceInternal( const sirVirtualTextureFeedbackReduceInput_t& input, const sirVirtualTextureFeedbackPolicy_t& policy,
	                     sirVirtualTextureFeedbackReductionStats_t *pStats, bool bMulti ) noexcept;
	void visit( uint32_t nPage ) noexcept;
	void swap( uint32_t nLeft, uint32_t nRight ) noexcept;
	void siftUp( uint32_t nIndex ) noexcept;
	void siftDown( uint32_t nIndex, uint32_t nCount ) noexcept;

	uint32_t m_nPageCapacity = 0;
	uint32_t m_nCandidateCapacity = 0;
	uint32_t m_nEpoch = 0;
	uint32_t m_nTouched = 0;
	uint32_t m_nSelected = 0;
	sirVirtualTextureFeedbackReducerBuffers_t m_Buffers = {};
};

#endif // __SIRENGINE_SIRVIRTUALTEXTUREFEEDBACKREDUCER_H__

/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_PREDICTIONSYSTEM_H__
#define __SIR_PREDICTIONSYSTEM_H__
#include <Streaming/SIRStreamingTypes.h>
typedef struct sirPredictionCandidate_s { sirStreamingUnitKey_t key; uint8_t nConfidence; uint64_t nDeadlineTick; uint32_t nBytes; } sirPredictionCandidate_t;
typedef struct sirPredictionConfig_s { sirPredictionCandidate_t *pCandidates; uint32_t nCapacity; uint32_t nMaxPrefetchBytes; uint64_t nConsumerId; } sirPredictionConfig_t;
typedef struct sirPredictionContext_s { const sirPredictionCandidate_t *pCandidates; uint32_t nCount; uint64_t nTick; uint32_t nEpoch; } sirPredictionContext_t;
typedef struct sirPredictionFeedback_s { sirStreamingUnitKey_t key; uint32_t nUsefulBytes; uint32_t nWastedBytes; } sirPredictionFeedback_t;
class SIRPredictionSystem final
{
public:
	sirStreamingResult_t initialize( const sirPredictionConfig_t& ) noexcept;
	sirStreamingResult_t update( const sirPredictionContext_t& ) noexcept;
	sirStreamingResult_t submitFeedback( const sirPredictionFeedback_t& ) noexcept;
	uint32_t collectDemands( sirStreamingDemand_t*, uint32_t ) noexcept;
	void reset( void ) noexcept;
private: sirPredictionCandidate_t *m_pCandidates = nullptr; uint32_t m_nCapacity = 0, m_nCount = 0, m_nBudget = 0; uint64_t m_nConsumer = 0; bool m_bInitialized = false;
};
#endif

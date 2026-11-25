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

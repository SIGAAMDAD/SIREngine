#include <Streaming/SIRPredictionSystem.h>

sirStreamingResult_t SIRPredictionSystem::initialize( const sirPredictionConfig_t& c ) noexcept
{
	if ( m_bInitialized || c.pCandidates == nullptr || c.nCapacity == 0 || c.nConsumerId == 0 ) return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	m_pCandidates = c.pCandidates; m_nCapacity = c.nCapacity; m_nBudget = c.nMaxPrefetchBytes; m_nConsumer = c.nConsumerId; m_nCount = 0; m_bInitialized = true; return SIRStreamingResult::SIR_STREAMING_OK;
}

sirStreamingResult_t SIRPredictionSystem::update( const sirPredictionContext_t& c ) noexcept
{
	if ( !m_bInitialized || c.pCandidates == nullptr || c.nCount > m_nCapacity ) return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;
	m_nCount = 0; uint32_t nBytes = 0;
	for ( uint32_t i = 0; i < c.nCount && m_nCount < m_nCapacity; ++i ) {
		if ( c.pCandidates[i].nBytes > m_nBudget - nBytes ) continue;
		m_pCandidates[m_nCount++] = c.pCandidates[i]; nBytes += c.pCandidates[i].nBytes;
	}
	return SIRStreamingResult::SIR_STREAMING_OK;
}

sirStreamingResult_t SIRPredictionSystem::submitFeedback( const sirPredictionFeedback_t& ) noexcept
{
	return m_bInitialized ? SIRStreamingResult::SIR_STREAMING_OK : SIRStreamingResult::SIR_STREAMING_NOT_READY;
}

uint32_t SIRPredictionSystem::collectDemands( sirStreamingDemand_t *p, uint32_t nCapacity ) noexcept
{
	if ( !m_bInitialized || p == nullptr ) return 0;
	uint32_t n = 0;
	for ( uint32_t i = 0; i < m_nCount && n < nCapacity; ++i ) p[n++] = { m_pCandidates[i].key, m_nConsumer, m_pCandidates[i].nDeadlineTick, 0, 0, SIRStreamingTarget::SIR_STREAMING_TARGET_PREPARED, m_pCandidates[i].nConfidence, 0 };
	m_nCount = 0;
	return n;
}

void SIRPredictionSystem::reset( void ) noexcept { m_nCount = 0; }

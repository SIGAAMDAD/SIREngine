#ifndef __SIR_IOREQUESTPLANNER_H__
#define __SIR_IOREQUESTPLANNER_H__
#include <Core/IO/SIRIOTypes.h>
class SIRIORequestPlanner final
{
public:
	SIRIORequestPlanner( void ) noexcept;
	sirIOResult_t initialize( const sirIOPlannerConfig_t& config ) noexcept;
	sirIOResult_t enqueue( const sirIOPlanInput_t& input ) noexcept;
	sirIOResult_t buildBatch( const sirIOBatchBudget_t& budget, sirIOPlannedBatch_t *pBatch ) noexcept;
	sirIOResult_t commitBatch( sirIOHandle_t handle ) noexcept;
	sirIOResult_t discardBatch( sirIOHandle_t handle ) noexcept;
	void reset( void ) noexcept;
private: sirIOPlanInput_t *m_pPending; uint32_t m_nCount,m_nCapacity,m_nNextGeneration,m_nBuiltCount; uint32_t m_nMergeGap; uint64_t m_nMaxMerged; bool m_bInitialized,m_bBuilt;
};
#endif // __SIR_IOREQUESTPLANNER_H__

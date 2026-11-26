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

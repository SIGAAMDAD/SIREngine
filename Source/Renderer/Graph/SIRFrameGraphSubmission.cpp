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

#include <Renderer/Graph/SIRFrameGraphSubmission.h>

sirGraphicsResult_t SIRFrameGraph::buildSubmissionPlan( const sirFrameGraphCompiled_t& compiled,
														sirFrameGraphSubmissionPlan_t *pPlan ) const noexcept
{
	if ( pPlan == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	memset( pPlan, 0, sizeof( *pPlan ) );
	if ( compiled.pOwner != this || compiled.nOwnerIdentity != m_nIdentity || compiled.nRevision != m_nRevision ||
		 compiled.nOriginalPassCount != m_nPassCount || compiled.nPassCount > m_nPassCount ||
		 compiled.nTransitionCount > SIR_FRAMEGRAPH_MAX_TRANSITIONS || compiled.nDependencyCount > SIR_FRAMEGRAPH_MAX_DEPENDENCIES ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	uint32_t szOrder[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		szOrder[iPass] = SIR_FRAMEGRAPH_INVALID_INDEX;
	}
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		if ( iPass >= m_nPassCount || !compiled.szLivePasses[iPass] || szOrder[iPass] != SIR_FRAMEGRAPH_INVALID_INDEX ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szOrder[iPass] = iOrder;
	}
	for ( uint32_t iPass = 0; iPass < m_nPassCount; ++iPass ) {
		if ( ( szOrder[iPass] != SIR_FRAMEGRAPH_INVALID_INDEX ) != ( compiled.szLivePasses[iPass] != 0 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	sirFrameGraphSubmissionPlan_t staged = {};
	staged.nPassCount = compiled.nPassCount;
	for ( uint32_t iOrder = 0; iOrder < compiled.nPassCount; ++iOrder ) {
		const uint32_t iPass = compiled.szPassOrder[iOrder];
		sirFrameGraphSubmissionPass_t& pass = staged.szPasses[iOrder];
		pass.nPass = iPass;
		pass.eQueue = m_szPasses[iPass].eQueue;
		pass.nBarrierBegin = staged.nBarrierCount;
		pass.nWaitBegin = staged.nWaitCount;
		for ( uint32_t iTransition = 0; iTransition < compiled.nTransitionCount; ++iTransition ) {
			const sirFrameGraphTransition_t& transition = compiled.szTransitions[iTransition];
			if ( transition.nResource >= m_nResourceCount || transition.nAfterPass >= m_nPassCount ||
				 szOrder[transition.nAfterPass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
				 transition.eToQueue != m_szPasses[transition.nAfterPass].eQueue ||
				 ( transition.nBeforePass != SIR_FRAMEGRAPH_INVALID_INDEX &&
				   ( transition.nBeforePass >= m_nPassCount || szOrder[transition.nBeforePass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
					 szOrder[transition.nBeforePass] >= szOrder[transition.nAfterPass] ||
					 transition.eFromQueue != m_szPasses[transition.nBeforePass].eQueue ) ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( transition.nAfterPass == iPass ) {
				staged.szBarrierIndices[staged.nBarrierCount++] = iTransition;
				++pass.nBarrierCount;
			}
		}
		for ( uint32_t iDependency = 0; iDependency < compiled.nDependencyCount; ++iDependency ) {
			const sirFrameGraphDependency_t& dependency = compiled.szDependencies[iDependency];
			if ( dependency.nBeforePass >= m_nPassCount || dependency.nAfterPass >= m_nPassCount ||
				 szOrder[dependency.nBeforePass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
				 szOrder[dependency.nAfterPass] == SIR_FRAMEGRAPH_INVALID_INDEX ||
				 szOrder[dependency.nBeforePass] >= szOrder[dependency.nAfterPass] ||
				 dependency.bCrossQueue != ( m_szPasses[dependency.nBeforePass].eQueue != m_szPasses[dependency.nAfterPass].eQueue ) ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( dependency.nAfterPass == iPass && dependency.bCrossQueue ) {
				bool bAlreadyWaited = false;
				for ( uint32_t iWait = pass.nWaitBegin; iWait < staged.nWaitCount; ++iWait ) {
					if ( staged.szWaits[iWait].nProducerPass == dependency.nBeforePass ) {
						bAlreadyWaited = true;
						break;
					}
				}
				if ( !bAlreadyWaited ) {
					sirFrameGraphSubmissionWait_t& wait = staged.szWaits[staged.nWaitCount++];
					wait.nProducerPass = dependency.nBeforePass;
					wait.nConsumerPass = dependency.nAfterPass;
					wait.eProducerQueue = m_szPasses[dependency.nBeforePass].eQueue;
					wait.eConsumerQueue = m_szPasses[dependency.nAfterPass].eQueue;
					++pass.nWaitCount;
				}
			}
		}
	}
	if ( staged.nBarrierCount != compiled.nTransitionCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pPlan = staged;
	return SIR_GRAPHICS_SUCCESS;
}

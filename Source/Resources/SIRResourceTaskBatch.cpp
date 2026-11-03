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

#include <Resources/SIRResourceTaskBatch.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct sirResourceTaskNode_s
{
	SIRResourceDependencyPlanEntry plan;
	uint64_t nInvalidationSequence;
	uint32_t iFirstDependent;
	uint32_t nPendingPrerequisites;
	uint32_t nLastDependent;
	sirResourceTaskState_t eState;
} sirResourceTaskNode_t;

typedef struct sirResourceTaskEdge_s
{
	uint32_t iDependent;
	uint32_t iNext;
} sirResourceTaskEdge_t;

SIRResourceTaskBatch::SIRResourceTaskBatch( SIRResourceGraph &rGraph, const sirResourceTaskConfig_t &rConfig ) noexcept
    : m_pGraph( &rGraph ), m_pWriter( rGraph.getWriter() ), m_Config( rConfig ), m_OwnerThread( std::this_thread::get_id() ),
      m_pNodes( nullptr ), m_pEdges( nullptr ), m_pPlan( nullptr ), m_Completions(), m_Stats{}, m_nMaxInFlight( 0 ),
      m_bReady( false ), m_bDispatching( false )
{
	if ( !rGraph.isReady() || m_pWriter == nullptr || !rConfig.allocator.m_pfnAllocate || !rConfig.allocator.m_pfnFree ||
	     !rConfig.callbacks.pfnSubmit || rConfig.nMaxTasks == 0 || rConfig.nMaxTasks > INT32_MAX ) {
		return;
	}
	const uint64_t nDesiredEdges = rConfig.nMaxEdges ? rConfig.nMaxEdges : ( uint64_t )rConfig.nMaxTasks * 4;
	m_nMaxInFlight = rConfig.nMaxInFlight ? rConfig.nMaxInFlight : 64;
	if ( nDesiredEdges == 0 || nDesiredEdges > UINT32_MAX || m_nMaxInFlight > COMPLETION_CAPACITY ) {
		return;
	}
	m_Config.nMaxEdges = ( uint32_t )nDesiredEdges;

	const uint64_t nNodesBytes = ( uint64_t )rConfig.nMaxTasks * sizeof( sirResourceTaskNode_t );
	const uint64_t nEdgeBytes = nDesiredEdges * sizeof( sirResourceTaskEdge_t );
	const uint64_t nPlanBytes = ( uint64_t )rConfig.nMaxTasks * sizeof( SIRResourceDependencyPlanEntry );
	if ( nNodesBytes > SIZE_MAX || nEdgeBytes > SIZE_MAX || nPlanBytes > SIZE_MAX ) {
		return;
	}
	m_pNodes = ( sirResourceTaskNode_t * )m_Config.allocator.m_pfnAllocate( m_Config.allocator.m_pContext, nNodesBytes,
	                                                                       alignof( sirResourceTaskNode_t ), m_Config.nMetadataDomain );
	m_pEdges = ( sirResourceTaskEdge_t * )m_Config.allocator.m_pfnAllocate( m_Config.allocator.m_pContext, nEdgeBytes,
	                                                                       alignof( sirResourceTaskEdge_t ), m_Config.nMetadataDomain );
	m_pPlan = ( SIRResourceDependencyPlanEntry * )m_Config.allocator.m_pfnAllocate( m_Config.allocator.m_pContext, nPlanBytes,
	                                                                                alignof( SIRResourceDependencyPlanEntry ),
	                                                                                m_Config.nMetadataDomain );
	m_bReady = m_pNodes != nullptr && m_pEdges != nullptr && m_pPlan != nullptr;
}

SIRResourceTaskBatch::~SIRResourceTaskBatch( void ) noexcept
{
	// Stop the external executor and join every producer before this point.
	// Cancelling a batch does not prove an accepted job stopped running.
	if ( m_Stats.nInFlight != 0 || m_bDispatching ) {
		abort();
	}
	m_Completions.stop();
	releaseMetadata();
}

bool SIRResourceTaskBatch::isWriterThread( void ) const noexcept
{
	return m_OwnerThread == std::this_thread::get_id();
}

bool SIRResourceTaskBatch::isReady( void ) const noexcept
{
	return m_bReady;
}

bool SIRResourceTaskBatch::isComplete( void ) const noexcept
{
	const uint64_t nResolved = ( uint64_t )m_Stats.nCompleted + m_Stats.nFailed + m_Stats.nSkipped;
	return m_Stats.bActive && m_Stats.nInFlight == 0 && nResolved == m_Stats.nTasks;
}

void SIRResourceTaskBatch::releaseMetadata( void ) noexcept
{
	const uint64_t nNodesBytes = ( uint64_t )m_Config.nMaxTasks * sizeof( sirResourceTaskNode_t );
	const uint64_t nEdgeBytes = ( uint64_t )m_Config.nMaxEdges * sizeof( sirResourceTaskEdge_t );
	const uint64_t nPlanBytes = ( uint64_t )m_Config.nMaxTasks * sizeof( SIRResourceDependencyPlanEntry );
	if ( m_pPlan != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pPlan, nPlanBytes,
		                             alignof( SIRResourceDependencyPlanEntry ), m_Config.nMetadataDomain );
		m_pPlan = nullptr;
	}
	if ( m_pEdges != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pEdges, nEdgeBytes,
		                             alignof( sirResourceTaskEdge_t ), m_Config.nMetadataDomain );
		m_pEdges = nullptr;
	}
	if ( m_pNodes != nullptr ) {
		m_Config.allocator.m_pfnFree( m_Config.allocator.m_pContext, m_pNodes, nNodesBytes,
		                             alignof( sirResourceTaskNode_t ), m_Config.nMetadataDomain );
		m_pNodes = nullptr;
	}
	m_bReady = false;
}

SIRResourceResult SIRResourceTaskBatch::buildEdges( void ) noexcept
{
	SIRResourceReadScope scope( *m_pGraph );
	if ( !scope.isActive() ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	for ( uint32_t iNode = 0; iNode < m_Stats.nTasks; ++iNode ) {
		sirResourceTaskNode_t &rNode = m_pNodes[iNode];
		SIRResourceView view;
		if ( m_pGraph->resolve( scope, rNode.plan.hResource, &view ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_NOT_RESIDENT;
		}
		if ( view.getRevision() != rNode.plan.nRevision ) {
			return SIR_RESOURCE_RESULT_STALE_REVISION;
		}
		SIRResourceInfo info = {};
		if ( view.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_FAILED;
		}
		if ( m_pWriter->getInvalidationSequence( rNode.plan.hResource, &rNode.nInvalidationSequence ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		for ( uint32_t iEdge = 0; iEdge < info.m_nDependencyCount; ++iEdge ) {
			SIRResourceDependency dependency = {};
			if ( view.getDependency( iEdge, &dependency ) != SIR_RESOURCE_RESULT_SUCCESS ) {
				return SIR_RESOURCE_RESULT_FAILED;
			}
			SIRResourceHandle hDependency = {};
			const SIRResourceResult eFound = m_pGraph->find( scope, dependency.m_Target, &hDependency );
			if ( eFound != SIR_RESOURCE_RESULT_SUCCESS ) {
				if ( dependency.m_eKind == SIR_RESOURCE_EDGE_KIND_REQUIRED ) {
					return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
				}
				continue;
			}
			uint32_t iPrerequisite = UINT32_MAX;
			for ( uint32_t iPrevious = 0; iPrevious < iNode; ++iPrevious ) {
				const SIRResourceHandle &rHandle = m_pNodes[iPrevious].plan.hResource;
				if ( rHandle.m_nSlot == hDependency.m_nSlot && rHandle.m_nGeneration == hDependency.m_nGeneration ) {
					iPrerequisite = iPrevious;
					break;
				}
			}
			if ( iPrerequisite == UINT32_MAX ) {
				return SIR_RESOURCE_RESULT_DEPENDENCY_CYCLE;
			}
			sirResourceTaskNode_t &rPrerequisite = m_pNodes[iPrerequisite];
			if ( rPrerequisite.nLastDependent == iNode ) {
				continue;
			}
			if ( m_Stats.nEdges >= m_Config.nMaxEdges ) {
				return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
			}
			rPrerequisite.nLastDependent = iNode;
			sirResourceTaskEdge_t &rNewEdge = m_pEdges[m_Stats.nEdges++];
			rNewEdge.iDependent = iNode;
			rNewEdge.iNext = rPrerequisite.iFirstDependent;
			rPrerequisite.iFirstDependent = m_Stats.nEdges - 1;
			++rNode.nPendingPrerequisites;
		}
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceTaskBatch::start( SIRResourceHandle hRoot ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	if ( m_bDispatching || ( m_Stats.bActive && !isComplete() ) ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	if ( m_Stats.nBatchGeneration == UINT32_MAX ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	uint32_t nRequired = 0;
	const SIRResourceResult ePlan = m_pWriter->compileDependencyOrder( hRoot, m_pPlan, m_Config.nMaxTasks, &nRequired );
	if ( ePlan != SIR_RESOURCE_RESULT_SUCCESS ) {
		return ePlan;
	}
	if ( nRequired == 0 ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const uint32_t nNextGeneration = m_Stats.nBatchGeneration + 1;
	m_Stats = {};
	m_Stats.nBatchGeneration = nNextGeneration;
	m_Stats.nTasks = nRequired;

	for ( uint32_t iNode = 0; iNode < nRequired; ++iNode ) {
		sirResourceTaskNode_t &rNode = m_pNodes[iNode];
		rNode = {};
		rNode.plan = m_pPlan[iNode];
		rNode.iFirstDependent = UINT32_MAX;
		rNode.nLastDependent = UINT32_MAX;
		rNode.eState = SIR_RESOURCE_TASK_WAITING;
	}
	const SIRResourceResult eBuild = buildEdges();
	if ( eBuild != SIR_RESOURCE_RESULT_SUCCESS ) {
		m_Stats.nTasks = 0;
		m_Stats.nEdges = 0;
		return eBuild;
	}
	m_Stats.bActive = true;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceTaskBatch::validateWorkItem( const sirResourceTaskWorkItem_t &rWork ) noexcept
{
	const uint32_t iNode = ( uint32_t )( rWork.nTicket & UINT32_MAX ) - 1;
	if ( iNode >= m_Stats.nTasks ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	SIRResourceReadScope scope( *m_pGraph );
	if ( !scope.isActive() ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRResourceView view;
	if ( m_pGraph->resolve( scope, rWork.hResource, &view ) != SIR_RESOURCE_RESULT_SUCCESS ||
	     view.getRevision() != rWork.nExpectedRevision ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	uint64_t nInvalidation = 0;
	const SIRResourceResult eInvalidation = m_pWriter->getInvalidationSequence( rWork.hResource, &nInvalidation );
	if ( eInvalidation != SIR_RESOURCE_RESULT_SUCCESS || nInvalidation != m_pNodes[iNode].nInvalidationSequence ) {
		return SIR_RESOURCE_RESULT_STALE_REVISION;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

void SIRResourceTaskBatch::skipPending( void ) noexcept
{
	m_Stats.bCancelled = true;
	for ( uint32_t iNode = 0; iNode < m_Stats.nTasks; ++iNode ) {
		if ( m_pNodes[iNode].eState == SIR_RESOURCE_TASK_WAITING ) {
			m_pNodes[iNode].eState = SIR_RESOURCE_TASK_SKIPPED;
			++m_Stats.nSkipped;
		}
	}
}

SIRResourceResult SIRResourceTaskBatch::dispatchReady( uint32_t nBudget, uint32_t *pSubmitted ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || pSubmitted == nullptr || !m_Stats.bActive ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pSubmitted = 0;
	if ( m_bDispatching || m_Stats.bCancelled ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	m_bDispatching = true;
	for ( uint32_t iNode = 0; iNode < m_Stats.nTasks && *pSubmitted < nBudget; ++iNode ) {
		sirResourceTaskNode_t &rNode = m_pNodes[iNode];
		if ( rNode.eState != SIR_RESOURCE_TASK_WAITING || rNode.nPendingPrerequisites != 0 ) {
			continue;
		}
		if ( m_Stats.nInFlight >= m_nMaxInFlight ) {
			break;
		}
		const sirResourceTaskWorkItem_t work = {
		    rNode.plan.hResource, rNode.plan.nRevision, ( ( uint64_t )m_Stats.nBatchGeneration << 32 ) | ( uint64_t )( iNode + 1 ) };
		const SIRResourceResult eValid = validateWorkItem( work );
		if ( eValid != SIR_RESOURCE_RESULT_SUCCESS ) {
			skipPending();
			m_bDispatching = false;
			return eValid;
		}

		rNode.eState = SIR_RESOURCE_TASK_SUBMITTED;
		++m_Stats.nInFlight;
		if ( !m_Config.callbacks.pfnSubmit( m_Config.callbacks.pContext, work ) ) {
			--m_Stats.nInFlight;
			rNode.eState = SIR_RESOURCE_TASK_WAITING;
			++m_Stats.nSubmitRejected;
			continue;
		}
		++( *pSubmitted );
	}
	m_bDispatching = false;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

sirEventEnqueueStatus_t SIRResourceTaskBatch::tryPostCompletion( const sirResourceTaskCompletion_t &rCompletion ) noexcept
{
	return m_Completions.tryEnqueue( rCompletion );
}

SIRResourceResult SIRResourceTaskBatch::drainCompletions( uint32_t nBudget, uint32_t *pConsumed ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( pConsumed == nullptr || !m_bReady || m_bDispatching ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pConsumed = 0;
	sirResourceTaskCompletion_t completion = {};
	while ( *pConsumed < nBudget && m_Completions.tryDequeue( completion ) ) {
		++( *pConsumed );
		const uint32_t nGeneration = ( uint32_t )( completion.nTicket >> 32 );
		const uint32_t nIndex = ( uint32_t )completion.nTicket;
		if ( nGeneration != m_Stats.nBatchGeneration || nIndex == 0 || nIndex > m_Stats.nTasks || !m_Stats.bActive ) {
			++m_Stats.nIgnoredCompletions;
			continue;
		}
		sirResourceTaskNode_t &rNode = m_pNodes[nIndex - 1];
		if ( rNode.eState != SIR_RESOURCE_TASK_SUBMITTED ) {
			++m_Stats.nIgnoredCompletions;
			continue;
		}
		--m_Stats.nInFlight;
		if ( m_Stats.bCancelled ) {
			rNode.eState = SIR_RESOURCE_TASK_SKIPPED;
			++m_Stats.nSkipped;
			continue;
		}
		const sirResourceTaskWorkItem_t work = { rNode.plan.hResource, rNode.plan.nRevision, completion.nTicket };
		const SIRResourceResult eValid = validateWorkItem( work );
		if ( completion.eResult != SIR_RESOURCE_RESULT_SUCCESS || eValid != SIR_RESOURCE_RESULT_SUCCESS ) {
			rNode.eState = SIR_RESOURCE_TASK_FAILED;
			++m_Stats.nFailed;
			skipPending();
			continue;
		}
		rNode.eState = SIR_RESOURCE_TASK_SUCCEEDED;
		++m_Stats.nCompleted;
		for ( uint32_t iEdge = rNode.iFirstDependent; iEdge != UINT32_MAX; iEdge = m_pEdges[iEdge].iNext ) {
			sirResourceTaskNode_t &rDependent = m_pNodes[m_pEdges[iEdge].iDependent];
			if ( rDependent.nPendingPrerequisites != 0 ) {
				--rDependent.nPendingPrerequisites;
			}
		}
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceTaskBatch::cancel( void ) noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bReady || !m_Stats.bActive ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( m_bDispatching ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	skipPending();
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceTaskBatch::getStats( sirResourceTaskStats_t *pStats ) const noexcept
{
	if ( !isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( pStats == nullptr || !m_bReady ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pStats = m_Stats;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

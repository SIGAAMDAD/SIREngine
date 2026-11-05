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

#include <Core/Jobs/SIRTaskFenceTracker.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <limits.h>
#include <new>
#include <stdlib.h>

typedef struct sirTaskFenceRecord_s
{
	sirTaskHandle_t hTask;
	uint64_t nFence;
	sirTaskFenceFinalize_t pfnFinalize;
	void *pContext;
	bool bFinalized;
} sirTaskFenceRecord_t;

SIRTaskFenceTracker::SIRTaskFenceTracker( void ) noexcept
    : m_pScheduler( nullptr ), m_pRecords( nullptr ), m_OwnerThread(), m_eProvider( SIR_TASK_EXEC_RENDER ), m_nLastTrackedFence( 0 ),
      m_nCompletedFence( 0 ), m_nCapacity( 0 ), m_nHead( 0 ), m_nPending( 0 ), m_bDraining( false ), m_bInitialized( false ),
      m_bEverInitialized( false )
{
}

SIRTaskFenceTracker::~SIRTaskFenceTracker()
{
	if ( m_bInitialized && !shutdown() ) {
		abort();
	}
}

bool SIRTaskFenceTracker::init( SIRMemoryManager *pMemory, SIRTaskScheduler *pScheduler, sirTaskExecutor_t eProvider,
                                uint32_t nCapacity ) noexcept
{
	if ( m_bEverInitialized || pMemory == nullptr || !pMemory->isInitialized() || pScheduler == nullptr || nCapacity == 0 ||
	     eProvider < SIR_TASK_EXEC_RENDER || eProvider >= SIR_TASK_EXEC_COUNT || !pScheduler->bindProviderOwner( eProvider ) ) {
		return false;
	}
	sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_RENDERING_CPU;
	if ( eProvider == SIR_TASK_EXEC_PHYSICS ) {
		eDomain = SIR_MEMORY_DOMAIN_PHYSICS;
	} else if ( eProvider == SIR_TASK_EXEC_AUDIO ) {
		eDomain = SIR_MEMORY_DOMAIN_AUDIO;
	}
	void *pRecords =
	    pMemory->allocateMetadata( ( uint64_t )nCapacity * sizeof( sirTaskFenceRecord_t ), alignof( sirTaskFenceRecord_t ), eDomain );
	if ( pRecords == nullptr ) {
		return false;
	}
	m_pRecords = ( sirTaskFenceRecord_t * )pRecords;
	m_pScheduler = pScheduler;
	m_OwnerThread = std::this_thread::get_id();
	m_eProvider = eProvider;
	m_nCapacity = nCapacity;
	m_nHead = 0;
	m_nPending = 0;
	m_nLastTrackedFence = 0;
	m_nCompletedFence = 0;
	m_bInitialized = true;
	m_bEverInitialized = true;
	return true;
}

bool SIRTaskFenceTracker::isOwner( void ) const noexcept
{
	return m_bInitialized && std::this_thread::get_id() == m_OwnerThread;
}

bool SIRTaskFenceTracker::shutdown( void ) noexcept
{
	if ( !isOwner() || m_nPending != 0 || m_bDraining ) {
		return false;
	}
	m_bInitialized = false;
	m_pScheduler = nullptr;
	m_pRecords = nullptr;
	return true;
}

bool SIRTaskFenceTracker::hasCapacity( void ) const noexcept
{
	return isOwner() && !m_bDraining && m_nPending < m_nCapacity;
}

uint32_t SIRTaskFenceTracker::getPendingCount( void ) const noexcept
{
	return isOwner() ? m_nPending : 0;
}

bool SIRTaskFenceTracker::track( sirTaskHandle_t task, uint64_t nFenceValue, sirTaskFenceFinalize_t pfnFinalize, void *pContext ) noexcept
{
	if ( !hasCapacity() || nFenceValue <= m_nCompletedFence || nFenceValue < m_nLastTrackedFence ) {
		return false;
	}
	sirTaskState_t eState = SIR_TASK_INVALID;
	sirTaskExecutor_t eExecutor = SIR_TASK_EXEC_CPU;
	if ( !m_pScheduler->queryTask( task, &eState, &eExecutor ) || eExecutor != m_eProvider ||
	     ( eState != SIR_TASK_RUNNING && eState != SIR_TASK_AWAITING_PROVIDER ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nPending; ++i ) {
		const sirTaskFenceRecord_t& record = m_pRecords[( m_nHead + i ) % m_nCapacity];
		if ( record.hTask.nSlot == task.nSlot && record.hTask.nGeneration == task.nGeneration ) {
			return false;
		}
	}
	const uint32_t nIndex = ( m_nHead + m_nPending ) % m_nCapacity;
	new ( m_pRecords + nIndex ) sirTaskFenceRecord_t{ task, nFenceValue, pfnFinalize, pContext, false };
	m_nLastTrackedFence = nFenceValue;
	++m_nPending;
	return true;
}

bool SIRTaskFenceTracker::untrackLast( sirTaskHandle_t task ) noexcept
{
	if ( !isOwner() || m_bDraining || m_nPending == 0 ) {
		return false;
	}
	const uint32_t nLast = ( m_nHead + m_nPending - 1 ) % m_nCapacity;
	const sirTaskFenceRecord_t& record = m_pRecords[nLast];
	if ( record.hTask.nSlot != task.nSlot || record.hTask.nGeneration != task.nGeneration || record.bFinalized ||
	     m_pScheduler->getState( task ) != SIR_TASK_RUNNING ) {
		return false;
	}
	--m_nPending;
	m_nLastTrackedFence = m_nPending == 0 ? m_nCompletedFence : m_pRecords[( m_nHead + m_nPending - 1 ) % m_nCapacity].nFence;
	return true;
}

bool SIRTaskFenceTracker::drain( uint64_t nCompletedFence, sirTaskResult_t eResult, uint32_t *pnAcknowledged ) noexcept
{
	if ( pnAcknowledged == nullptr ) {
		return false;
	}
	*pnAcknowledged = 0;
	if ( !isOwner() || ( eResult != SIR_TASK_RESULT_SUCCESS && eResult != SIR_TASK_RESULT_FAILURE ) ||
	     nCompletedFence < m_nCompletedFence || m_bDraining ) {
		return false;
	}
	m_bDraining = true;
	m_nCompletedFence = nCompletedFence;
	while ( m_nPending != 0 ) {
		sirTaskFenceRecord_t& record = m_pRecords[m_nHead];
		if ( record.nFence > nCompletedFence ) {
			break;
		}
		if ( !record.bFinalized ) {
			if ( record.pfnFinalize != nullptr && !record.pfnFinalize( record.pContext ) ) {
				m_bDraining = false;
				return false;
			}
			record.bFinalized = true;
		}
		if ( !m_pScheduler->completeProvider( record.hTask, eResult ) ) {
			m_bDraining = false;
			return false;
		}
		m_nHead = ( m_nHead + 1 ) % m_nCapacity;
		--m_nPending;
		++*pnAcknowledged;
	}
	m_bDraining = false;
	return true;
}

bool SIRTaskFenceTracker::advanceCompleted( uint64_t nCompletedFence, sirTaskResult_t eResult, uint32_t *pnAcknowledged ) noexcept
{
	return drain( nCompletedFence, eResult, pnAcknowledged );
}

bool SIRTaskFenceTracker::failQuiesced( uint32_t *pnAcknowledged ) noexcept
{
	const uint64_t nFence = m_nLastTrackedFence > m_nCompletedFence ? m_nLastTrackedFence : m_nCompletedFence;
	return drain( nFence, SIR_TASK_RESULT_FAILURE, pnAcknowledged );
}

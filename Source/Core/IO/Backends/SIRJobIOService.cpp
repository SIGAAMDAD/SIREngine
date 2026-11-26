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

#include <Core/IO/Backends/SIRJobIOService.h>

struct SIRJobIOService::Record
{
	sirIORequest_t request;
	sirIOCompletion_t completion;
	sirTaskHandle_t task;
	uint32_t generation;
	bool active;
	bool delivered;
};

SIRJobIOService::SIRJobIOService( void ) noexcept : m_pScheduler( nullptr ), m_pRecords( nullptr ), m_nCapacity( 0 ), m_bInitialized( false ) {}

SIRTaskResult SIRJobIOService::run( void *pContext, sirTaskHandle_t ) noexcept
{
	Record *pRecord = static_cast<Record *>( pContext );
	pRecord->completion.nTransferred = 0;
	if ( pRecord->request.pSource == nullptr || pRecord->request.pReadAt == nullptr ) {
		pRecord->completion.eResult = SIR_IO_INVALID_ARGUMENT;
		return SIRTaskResult::SIR_TASK_RESULT_FAILURE;
	}
	sirIOBytes_t destination = pRecord->request.destination;
	destination.nSize = pRecord->request.nBytes;
	uint64_t nTransferred = 0;
	const sirIOResult_t result = pRecord->request.pReadAt( pRecord->request.pSource, pRecord->request.nOffset, destination, &nTransferred );
	pRecord->completion.nTransferred = nTransferred;
	pRecord->completion.eResult = result;
	return result == SIR_IO_OK ? SIRTaskResult::SIR_TASK_RESULT_SUCCESS : SIRTaskResult::SIR_TASK_RESULT_FAILURE;
}

sirIOResult_t SIRJobIOService::initialize( const sirJobIOConfig_t& config ) noexcept
{
	if ( m_bInitialized || config.pScheduler == nullptr || config.pRecordStorage == nullptr || config.nRecordCapacity == 0 ) return SIR_IO_INVALID_ARGUMENT;
	m_pScheduler = config.pScheduler;
	m_pRecords = static_cast<Record *>( config.pRecordStorage );
	m_nCapacity = config.nRecordCapacity;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) m_pRecords[i] = {};
	m_bInitialized = true;
	return SIR_IO_OK;
}

sirIOResult_t SIRJobIOService::submit( const sirIORequest_t& request, sirIOHandle_t *pHandle ) noexcept
{
	if ( !m_bInitialized || pHandle == nullptr || request.pSource == nullptr || request.destination.pData == nullptr || request.nBytes == 0 ) return SIR_IO_INVALID_ARGUMENT;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
		Record& record = m_pRecords[i];
		if ( record.active ) continue;
		const uint32_t generation = record.generation + 1;
		record = {};
		record.generation = generation;
		record.request = request;
		record.active = true;
		record.completion.handle = { i, record.generation };
		sirTaskBatchTask_t task { &run, &record, SIRTaskPriority::SIR_TASK_PRIORITY_NORMAL, SIRTaskExecutor::SIR_TASK_EXEC_CPU };
		if ( !m_pScheduler->submit( task, &record.task ) ) {
			record.active = false;
			return SIR_IO_OUT_OF_CAPACITY;
		}
		*pHandle = record.completion.handle;
		return SIR_IO_OK;
	}
	return SIR_IO_OUT_OF_CAPACITY;
}

sirIOResult_t SIRJobIOService::submitBatch( const sirIOBatch_t& batch, sirIOHandle_t *pHandles ) noexcept
{
	if ( !m_bInitialized || batch.pRequests == nullptr || pHandles == nullptr || batch.nCount == 0 ) return SIR_IO_INVALID_ARGUMENT;
	uint32_t nFree = 0;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) nFree += !m_pRecords[i].active;
	if ( nFree < batch.nCount ) return SIR_IO_OUT_OF_CAPACITY;
	for ( uint32_t i = 0; i < batch.nCount; ++i ) if ( submit( batch.pRequests[i], &pHandles[i] ) != SIR_IO_OK ) return SIR_IO_FAILED;
	return SIR_IO_OK;
}

sirIOResult_t SIRJobIOService::cancel( sirIOHandle_t handle ) noexcept
{
	if ( !m_bInitialized || handle.nSlot >= m_nCapacity || !m_pRecords[handle.nSlot].active ) return SIR_IO_STALE_HANDLE;
	Record& record = m_pRecords[handle.nSlot];
	return m_pScheduler->cancel( record.task ) ? SIR_IO_CANCELLED : SIR_IO_NOT_READY;
}

uint32_t SIRJobIOService::collect( sirIOCompletion_t *pCompletions, uint32_t nCapacity ) noexcept
{
	if ( !m_bInitialized || pCompletions == nullptr ) return 0;
	uint32_t nCollected = 0;
	for ( uint32_t i = 0; i < m_nCapacity && nCollected < nCapacity; ++i ) {
		Record& record = m_pRecords[i];
		if ( !record.active || record.delivered ) continue;
		const SIRTaskState state = m_pScheduler->getState( record.task );
		if ( state != SIRTaskState::SIR_TASK_COMPLETED && state != SIRTaskState::SIR_TASK_FAILED && state != SIRTaskState::SIR_TASK_CANCELLED ) continue;
		if ( state == SIRTaskState::SIR_TASK_CANCELLED ) record.completion.eResult = SIR_IO_CANCELLED;
		pCompletions[nCollected++] = record.completion;
		record.delivered = true;
		m_pScheduler->releaseTask( record.task );
		record.active = false;
	}
	return nCollected;
}

sirIOResult_t SIRJobIOService::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) return SIR_IO_OK;
	for ( uint32_t i = 0; i < m_nCapacity; ++i ) if ( m_pRecords[i].active ) return SIR_IO_BUSY;
	m_pScheduler = nullptr;
	m_pRecords = nullptr;
	m_nCapacity = 0;
	m_bInitialized = false;
	return SIR_IO_OK;
}



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

#include <Resources/SIRResourceFileLoad.h>

#include <Core/Platform/SIRPlatformFileMapping.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

SIRResourceFileLoad::SIRResourceFileLoad( SIRResourceGraph &rGraph, SIRTaskScheduler &rScheduler ) noexcept
    : m_pGraph( &rGraph ), m_pScheduler( &rScheduler ), m_File(), m_Request{}, m_Staging{}, m_Bytes{}, m_Task{ UINT32_MAX, 0 },
      m_eTransferResult( SIR_FILE_OK ), m_eLastFileResult( SIR_FILE_OK ), m_bActive( false ), m_bDiscard( false )
{
}

SIRResourceFileLoad::~SIRResourceFileLoad( void ) noexcept
{
	// An accepted Core Jobs task may still be writing into GRDG staging.
	// Ownership cannot be released while that callback is in flight.
	if ( m_bActive ) {
		abort();
	}
}

bool SIRResourceFileLoad::isActive( void ) const noexcept
{
	return m_bActive;
}

sirFileResult_t SIRResourceFileLoad::getLastFileResult( void ) const noexcept
{
	return m_eLastFileResult;
}

bool SIRResourceFileLoad::getTaskHandle( sirTaskHandle_t *pHandle ) const noexcept
{
	if ( pHandle == nullptr || !m_bActive || m_pGraph->getWriter() == nullptr ) {
		return false;
	}
	*pHandle = m_Task;
	return true;
}

void SIRResourceFileLoad::clearStaging( void ) noexcept
{
	if ( m_Bytes.m_pData != nullptr ) {
		SIRResourceGraphWriter *pWriter = m_pGraph->getWriter();
		if ( pWriter == nullptr || pWriter->cancelStaging( m_Staging ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			abort();
		}
		m_Bytes = {};
		m_Staging = {};
	}
}

void SIRResourceFileLoad::closeTransfer( void ) noexcept
{
	m_File.close();
	m_Bytes = {};
	m_Staging = {};
	m_Task = { UINT32_MAX, 0 };
	m_Request = {};
	m_bActive = false;
	m_bDiscard = false;
}

SIRResourceResult SIRResourceFileLoad::begin( const char *pszUtf8Path, const sirResourceFileLoadRequest_t &rRequest ) noexcept
{
	SIRResourceGraphWriter *pWriter = m_pGraph->getWriter();
	if ( pWriter == nullptr ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( m_bActive ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	if ( !m_pGraph->isReady() || pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' || rRequest.keyResource.m_nTypeId == 0 ||
	     rRequest.eKind > SIR_RESOURCE_KIND_COMPOSITION || rRequest.eMode > SIR_RESOURCE_FILE_READ_MAPPED || rRequest.nSchemaVersion == 0 ||
	     rRequest.nFileBytes == 0 || rRequest.nFileBytes > SIZE_MAX || rRequest.nAlignment == 0 ||
	     ( rRequest.nAlignment & ( rRequest.nAlignment - 1 ) ) != 0 ||
	     ( rRequest.nDependencyCount != 0 && rRequest.pDependencies == nullptr ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	m_eLastFileResult = m_File.open( pszUtf8Path, SIR_FILE_OPEN_READ );
	if ( m_eLastFileResult != SIR_FILE_OK ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	uint64_t nFileSize = 0;
	m_eLastFileResult = m_File.getSize( &nFileSize );
	if ( m_eLastFileResult != SIR_FILE_OK ) {
		m_File.close();
		return SIR_RESOURCE_RESULT_FAILED;
	}
	if ( rRequest.nFileOffset >= nFileSize || rRequest.nFileBytes > nFileSize - rRequest.nFileOffset ||
	     rRequest.nFileOffset > ( uint64_t )INT64_MAX || rRequest.nFileBytes > ( uint64_t )INT64_MAX - rRequest.nFileOffset ) {
		m_eLastFileResult = SIR_FILE_OUT_OF_RANGE;
		m_File.close();
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceStagingHandle hStaging = {};
	SIRResourceMutableBytes bytes = {};
	const SIRResourceResult eStaging =
	    pWriter->allocateStaging( rRequest.nFileBytes, rRequest.nAlignment, rRequest.nStagingDomain, &hStaging, &bytes );
	if ( eStaging != SIR_RESOURCE_RESULT_SUCCESS ) {
		m_File.close();
		return eStaging;
	}
	m_Request = rRequest;
	m_Staging = hStaging;
	m_Bytes = bytes;
	m_eTransferResult = SIR_FILE_OK;
	m_bDiscard = false;
	m_bActive = true;
	m_Task = { UINT32_MAX, 0 };
	if ( !m_pScheduler->submit( &SIRResourceFileLoad::readTask, this, SIR_TASK_PRIORITY_BACKGROUND, SIR_TASK_EXEC_CPU, &m_Task ) ) {
		clearStaging();
		closeTransfer();
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

sirTaskResult_t SIRResourceFileLoad::readTask( void *pContext, sirTaskHandle_t hTask ) noexcept
{
	SIRResourceFileLoad *pLoad = ( SIRResourceFileLoad * )pContext;
	if ( pLoad->m_pScheduler->isCancellationRequested( hTask ) ) {
		return SIR_TASK_RESULT_FAILURE;
	}
	if ( pLoad->m_Request.eMode == SIR_RESOURCE_FILE_READ_MAPPED ) {
		SIRPlatformFileMapping mapping;
		pLoad->m_eTransferResult =
		    mapping.map( pLoad->m_File, pLoad->m_Request.nFileOffset, pLoad->m_Request.nFileBytes, SIR_FILE_MAP_READ_ONLY );
		if ( pLoad->m_eTransferResult != SIR_FILE_OK ) {
			return SIR_TASK_RESULT_FAILURE;
		}
		memcpy( pLoad->m_Bytes.m_pData, mapping.getData(), ( size_t )pLoad->m_Request.nFileBytes );
		pLoad->m_eTransferResult = mapping.unmap();
	} else {
		uint64_t nTransferred = 0;
		pLoad->m_eTransferResult =
		    pLoad->m_File.readAt( pLoad->m_Request.nFileOffset, pLoad->m_Bytes.m_pData, pLoad->m_Request.nFileBytes, &nTransferred );
	}
	return pLoad->m_eTransferResult == SIR_FILE_OK ? SIR_TASK_RESULT_SUCCESS : SIR_TASK_RESULT_FAILURE;
}

SIRResourceResult SIRResourceFileLoad::cancel( void ) noexcept
{
	if ( m_pGraph->getWriter() == nullptr ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bActive ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	m_bDiscard = true;
	const sirTaskState_t eState = m_pScheduler->getState( m_Task );
	if ( eState == SIR_TASK_QUEUED || eState == SIR_TASK_BLOCKED || eState == SIR_TASK_RUNNING ) {
		m_pScheduler->cancel( m_Task );
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceFileLoad::poll( SIRResourceHandle *pResource ) noexcept
{
	SIRResourceGraphWriter *pWriter = m_pGraph->getWriter();
	if ( pWriter == nullptr ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !m_bActive || pResource == nullptr ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const sirTaskState_t eState = m_pScheduler->getState( m_Task );
	if ( eState == SIR_TASK_QUEUED || eState == SIR_TASK_BLOCKED || eState == SIR_TASK_RUNNING || eState == SIR_TASK_AWAITING_PROVIDER ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	if ( eState != SIR_TASK_COMPLETED && eState != SIR_TASK_FAILED && eState != SIR_TASK_CANCELLED ) {
		// The task handle may have been externally released after completion
		// or invalidated by a scheduler shutdown. No worker retains staging.
		clearStaging();
		closeTransfer();
		return SIR_RESOURCE_RESULT_FAILED;
	}
	// Reading task state acquires the Core scheduler mutex: worker writes into
	// staging have completed before the sole GRDG writer inspects the buffer.
	if ( !m_pScheduler->releaseTask( m_Task ) ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	m_eLastFileResult = m_eTransferResult;
	SIRResourceResult eResult = SIR_RESOURCE_RESULT_FAILED;
	if ( !m_bDiscard && eState == SIR_TASK_COMPLETED && m_eTransferResult == SIR_FILE_OK ) {
		const SIRResourceChunkPublication chunk = { m_Request.idChunk, m_Request.keyContent, m_Staging, false };
		const SIRResourcePublication publication = {
		    m_Request.keyResource,   m_Request.eKind,           m_Request.nSchemaVersion, m_Request.nFormatId, &chunk, 1,
		    m_Request.pDependencies, m_Request.nDependencyCount };
		eResult = pWriter->publishExpected( publication, m_Request.nExpectedRevision, pResource );
		if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
			// Ownership moved to the canonical record; the staging slot is empty.
			m_Bytes = {};
			m_Staging = {};
		}
	}
	clearStaging();
	closeTransfer();
	return eResult;
}

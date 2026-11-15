/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties of
merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/IO/SIRAsyncIOService.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRScopedLock.h>

#if defined( __linux__ )
#include <Core/IO/SIRIOUringReader.h>
#endif
#if defined( _WIN32 )
#include <Core/IO/SIRIOCPReader.h>
#endif

#include <limits.h>
#include <new>

static constexpr uint32_t SIR_IO_NO_SLOT = UINT32_MAX;

typedef enum sirIOSlotState_e : uint8_t
{
	SIR_IO_SLOT_FREE = 0,
	SIR_IO_SLOT_QUEUED,
	SIR_IO_SLOT_RUNNING,
	SIR_IO_SLOT_TERMINAL,
	SIR_IO_SLOT_DELIVERED
} sirIOSlotState_t;

struct sirIOSourceSlot_s
{
	SIRPlatformFile file;
	uint64_t nBytes = 0;
	uint32_t nGeneration = 1;
	uint32_t nReferences = 0;
	bool bOpen = false;
};

struct sirIOBufferSlot_s
{
	uint8_t *pData = nullptr;
	uint64_t nBytes = 0;
	uint32_t nGeneration = 1;
	uint32_t nReferences = 0;
};

struct sirIORequestSlot_s
{
	sirIOReadRequest_t request = {};
	sirIOResult_t eResult = SIR_IO_NOT_READY;
	sirIOSlotState_t eState = SIR_IO_SLOT_FREE;
	uint32_t nGeneration = 1;
	uint64_t nTransferred = 0;
	uint64_t nSequence = 0;
	bool bCancelled = false;
};

static uintptr_t sirIOAlign( uintptr_t nValue, uintptr_t nAlignment ) noexcept
{
	return ( nValue + nAlignment - 1 ) & ~( nAlignment - 1 );
}

SIRAsyncIOService::SIRAsyncIOService( void ) noexcept
    : m_pSources( nullptr ), m_pBuffers( nullptr ), m_pRequests( nullptr ), m_nSourceCapacity( 0 ), m_nBufferCapacity( 0 ),
      m_nRequestCapacity( 0 ), m_nCompletionSequence( 0 ), m_bInitialized( false ), m_bClosing( false )
{
}

SIRAsyncIOService::~SIRAsyncIOService( void ) noexcept
{
	shutdown();
}

uint64_t SIRAsyncIOService::requiredBytes( uint32_t nRequests, uint32_t nSources, uint32_t nBuffers ) noexcept
{
	if ( nRequests == 0 || nSources == 0 || nBuffers == 0 || nRequests > 65536 || nSources > 65536 || nBuffers > 65536 ) {
		return 0;
	}
	return ( uint64_t )nRequests * sizeof( sirIORequestSlot_s ) + ( uint64_t )nSources * sizeof( sirIOSourceSlot_s ) +
	       ( uint64_t )nBuffers * sizeof( sirIOBufferSlot_s ) + 3 * alignof( sirIOSourceSlot_s ) + 32;
}

bool SIRAsyncIOService::initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nRequests, uint32_t nSources,
                                    uint32_t nBuffers ) noexcept
{
	const uint64_t nRequired = requiredBytes( nRequests, nSources, nBuffers );
	if ( m_bInitialized || pStorage == nullptr || nRequired == 0 || nStorageBytes < nRequired ||
	     nStorageBytes > UINTPTR_MAX - ( uintptr_t )pStorage ) {
		return false;
	}

	uintptr_t nCursor = ( uintptr_t )pStorage;
	nCursor = sirIOAlign( nCursor, alignof( sirIOSourceSlot_s ) );
	m_pSources = ( sirIOSourceSlot_s * )nCursor;
	nCursor += ( uint64_t )nSources * sizeof( sirIOSourceSlot_s );
	nCursor = sirIOAlign( nCursor, alignof( sirIOBufferSlot_s ) );
	m_pBuffers = ( sirIOBufferSlot_s * )nCursor;
	nCursor += ( uint64_t )nBuffers * sizeof( sirIOBufferSlot_s );
	nCursor = sirIOAlign( nCursor, alignof( sirIORequestSlot_s ) );
	m_pRequests = ( sirIORequestSlot_s * )nCursor;
	if ( nCursor + ( uint64_t )nRequests * sizeof( sirIORequestSlot_s ) > ( uintptr_t )pStorage + nStorageBytes ) {
		m_pSources = nullptr;
		m_pBuffers = nullptr;
		m_pRequests = nullptr;
		return false;
	}

	for ( uint32_t i = 0; i < nSources; ++i ) {
		new ( m_pSources + i ) sirIOSourceSlot_s();
	}
	for ( uint32_t i = 0; i < nBuffers; ++i ) {
		new ( m_pBuffers + i ) sirIOBufferSlot_s();
	}
	for ( uint32_t i = 0; i < nRequests; ++i ) {
		new ( m_pRequests + i ) sirIORequestSlot_s();
	}
	m_nSourceCapacity = nSources;
	m_nBufferCapacity = nBuffers;
	m_nRequestCapacity = nRequests;
	m_nCompletionSequence = 0;
	m_bClosing = false;
	m_bInitialized = true;
	if ( !m_Worker.start( &SIRAsyncIOService::workerEntry, this ) ) {
		shutdown();
		return false;
	}
	return true;
}

bool SIRAsyncIOService::validSourceLocked( sirIOSourceHandle_t hSource ) const noexcept
{
	return hSource.nSlot < m_nSourceCapacity && m_pSources[hSource.nSlot].bOpen &&
	       m_pSources[hSource.nSlot].nGeneration == hSource.nGeneration;
}

bool SIRAsyncIOService::validBufferLocked( sirIOBufferHandle_t hBuffer ) const noexcept
{
	return hBuffer.nSlot < m_nBufferCapacity && m_pBuffers[hBuffer.nSlot].pData != nullptr &&
	       m_pBuffers[hBuffer.nSlot].nGeneration == hBuffer.nGeneration;
}

bool SIRAsyncIOService::validRequestLocked( sirIORequestHandle_t hRequest ) const noexcept
{
	return hRequest.nSlot < m_nRequestCapacity && m_pRequests[hRequest.nSlot].eState != SIR_IO_SLOT_FREE &&
	       m_pRequests[hRequest.nSlot].nGeneration == hRequest.nGeneration;
}

sirIOResult_t SIRAsyncIOService::openSource( const char *szPath, sirIOSourceHandle_t *pHandle ) noexcept
{
	if ( szPath == nullptr || pHandle == nullptr || szPath[0] == '\0' ) {
		return SIR_IO_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || m_bClosing ) {
		return SIR_IO_SHUTTING_DOWN;
	}
	for ( uint32_t i = 0; i < m_nSourceCapacity; ++i ) {
		sirIOSourceSlot_s& slot = m_pSources[i];
		if ( slot.bOpen ) {
			continue;
		}
		if ( slot.file.open( szPath, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
			return SIR_IO_READ_FAILED;
		}
		if ( slot.file.getSize( &slot.nBytes ) != SIR_FILE_OK ) {
			slot.file.close();
			return SIR_IO_READ_FAILED;
		}
		slot.bOpen = true;
		*pHandle = { i, slot.nGeneration };
		return SIR_IO_OK;
	}
	return SIR_IO_OUT_OF_CAPACITY;
}

sirIOResult_t SIRAsyncIOService::closeSource( sirIOSourceHandle_t hSource ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || !validSourceLocked( hSource ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	sirIOSourceSlot_s& slot = m_pSources[hSource.nSlot];
	if ( slot.nReferences != 0 ) {
		return SIR_IO_BUSY;
	}
	slot.file.close();
	slot.bOpen = false;
	slot.nBytes = 0;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		++slot.nGeneration;
	}
	return SIR_IO_OK;
}

sirIOResult_t SIRAsyncIOService::inspectSource( sirIOSourceHandle_t hSource, void *pHeader, uint32_t nHeaderBytes,
                                                uint64_t *pSize ) noexcept
{
	if ( pHeader == nullptr || pSize == nullptr || nHeaderBytes == 0 || nHeaderBytes > 256 ) {
		return SIR_IO_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || !validSourceLocked( hSource ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	const sirIOSourceSlot_s& slot = m_pSources[hSource.nSlot];
	if ( nHeaderBytes > slot.nBytes ) {
		return SIR_IO_SHORT_READ;
	}
	uint64_t nTransferred = 0;
	const sirFileResult_t eResult = slot.file.readAt( 0, pHeader, nHeaderBytes, &nTransferred );
	if ( eResult != SIR_FILE_OK || nTransferred != nHeaderBytes ) {
		return SIR_IO_READ_FAILED;
	}
	*pSize = slot.nBytes;
	return SIR_IO_OK;
}

sirIOResult_t SIRAsyncIOService::registerBuffer( void *pData, uint64_t nBytes, sirIOBufferHandle_t *pHandle ) noexcept
{
	if ( pData == nullptr || nBytes == 0 || pHandle == nullptr || nBytes > SIZE_MAX ) {
		return SIR_IO_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || m_bClosing ) {
		return SIR_IO_SHUTTING_DOWN;
	}
	for ( uint32_t i = 0; i < m_nBufferCapacity; ++i ) {
		sirIOBufferSlot_s& slot = m_pBuffers[i];
		if ( slot.pData == nullptr ) {
			slot.pData = ( uint8_t * )pData;
			slot.nBytes = nBytes;
			*pHandle = { i, slot.nGeneration };
			return SIR_IO_OK;
		}
	}
	return SIR_IO_OUT_OF_CAPACITY;
}

sirIOResult_t SIRAsyncIOService::unregisterBuffer( sirIOBufferHandle_t hBuffer ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || !validBufferLocked( hBuffer ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	sirIOBufferSlot_s& slot = m_pBuffers[hBuffer.nSlot];
	if ( slot.nReferences != 0 ) {
		return SIR_IO_BUSY;
	}
	slot.pData = nullptr;
	slot.nBytes = 0;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		++slot.nGeneration;
	}
	return SIR_IO_OK;
}

sirIOResult_t SIRAsyncIOService::submit( const sirIOReadRequest_t& request, sirIORequestHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr || request.nBytes == 0 || request.nOffset > INT64_MAX || request.nBytes > INT64_MAX - request.nOffset ||
	     request.nPriority > 255 ) {
		return SIR_IO_INVALID_ARGUMENT;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || m_bClosing ) {
		return SIR_IO_SHUTTING_DOWN;
	}
	if ( !validSourceLocked( request.hSource ) || !validBufferLocked( request.hBuffer ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	const sirIOSourceSlot_s& source = m_pSources[request.hSource.nSlot];
	const sirIOBufferSlot_s& buffer = m_pBuffers[request.hBuffer.nSlot];
	if ( request.nOffset > source.nBytes || request.nBytes > source.nBytes - request.nOffset ||
	     request.nDestinationOffset > buffer.nBytes || request.nBytes > buffer.nBytes - request.nDestinationOffset ) {
		return SIR_IO_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < m_nRequestCapacity; ++i ) {
		sirIORequestSlot_s& slot = m_pRequests[i];
		if ( slot.eState != SIR_IO_SLOT_FREE ) {
			continue;
		}
		slot.request = request;
		slot.eResult = SIR_IO_NOT_READY;
		slot.eState = SIR_IO_SLOT_QUEUED;
		slot.nTransferred = 0;
		slot.nSequence = 0;
		slot.bCancelled = false;
		++m_pSources[request.hSource.nSlot].nReferences;
		++m_pBuffers[request.hBuffer.nSlot].nReferences;
		*pHandle = { i, slot.nGeneration };
		m_Ready.notifyOne();
		return SIR_IO_OK;
	}
	return SIR_IO_OUT_OF_CAPACITY;
}

sirIOResult_t SIRAsyncIOService::cancel( sirIORequestHandle_t hRequest ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || !validRequestLocked( hRequest ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	sirIORequestSlot_s& slot = m_pRequests[hRequest.nSlot];
	if ( slot.eState == SIR_IO_SLOT_DELIVERED || slot.eState == SIR_IO_SLOT_TERMINAL ) {
		return SIR_IO_BUSY;
	}
	slot.bCancelled = true;
	m_Ready.notifyOne();
	return SIR_IO_OK;
}

uint32_t SIRAsyncIOService::selectQueuedLocked( void ) const noexcept
{
	uint32_t iSelected = SIR_IO_NO_SLOT;
	for ( uint32_t i = 0; i < m_nRequestCapacity; ++i ) {
		const sirIORequestSlot_s& slot = m_pRequests[i];
		if ( slot.eState != SIR_IO_SLOT_QUEUED ) {
			continue;
		}
		if ( iSelected == SIR_IO_NO_SLOT || slot.bCancelled || slot.request.nPriority > m_pRequests[iSelected].request.nPriority ) {
			iSelected = i;
		}
	}
	return iSelected;
}

bool SIRAsyncIOService::hasRunningOrQueuedLocked( void ) const noexcept
{
	for ( uint32_t i = 0; i < m_nRequestCapacity; ++i ) {
		if ( m_pRequests[i].eState == SIR_IO_SLOT_RUNNING || m_pRequests[i].eState == SIR_IO_SLOT_QUEUED ) {
			return true;
		}
	}
	return false;
}

void SIRAsyncIOService::workerEntry( void *pContext ) noexcept
{
	( ( SIRAsyncIOService * )pContext )->workerLoop();
}

void SIRAsyncIOService::workerLoop( void ) noexcept
{
#if defined( __linux__ )
	SIRIOUringReader nativeReader;
	nativeReader.initialize();
#endif
#if defined( _WIN32 )
	SIRIOCPReader nativeReader;
	nativeReader.initialize();
#endif
	for ( ;; ) {
		m_Mutex.lock();
		uint32_t iSlot = selectQueuedLocked();
		while ( iSlot == SIR_IO_NO_SLOT && !m_bClosing ) {
			m_Ready.wait( m_Mutex );
			iSlot = selectQueuedLocked();
		}
		if ( iSlot == SIR_IO_NO_SLOT && m_bClosing ) {
			m_Mutex.unlock();
			break;
		}

		sirIORequestSlot_s& slot = m_pRequests[iSlot];
		const sirIOReadRequest_t request = slot.request;
		const bool bCancelled = slot.bCancelled || m_bClosing;
		slot.eState = SIR_IO_SLOT_RUNNING;
		m_Mutex.unlock();

		uint64_t nTransferred = 0;
		sirIOResult_t eResult = SIR_IO_CANCELLED;
		if ( !bCancelled ) {
#if defined( __linux__ )
			const sirFileResult_t eRead =
			    nativeReader.readAt( m_pSources[request.hSource.nSlot].file, request.nOffset,
				                     m_pBuffers[request.hBuffer.nSlot].pData + request.nDestinationOffset, request.nBytes, &nTransferred );
#elif defined( _WIN32 )
			const sirFileResult_t eRead =
			    nativeReader.readAt( m_pSources[request.hSource.nSlot].file, request.nOffset,
				                     m_pBuffers[request.hBuffer.nSlot].pData + request.nDestinationOffset, request.nBytes, &nTransferred );
#endif
			eResult = eRead == SIR_FILE_OK && nTransferred == request.nBytes ? SIR_IO_OK
			          : eRead == SIR_FILE_END_OF_FILE                        ? SIR_IO_SHORT_READ
			                                                                 : SIR_IO_READ_FAILED;
		}

		m_Mutex.lock();
		slot.eResult = slot.bCancelled || m_bClosing ? SIR_IO_CANCELLED : eResult;
		slot.nTransferred = nTransferred;
		slot.nSequence = ++m_nCompletionSequence;
		slot.eState = SIR_IO_SLOT_TERMINAL;
		m_Mutex.unlock();
	}
}

uint32_t SIRAsyncIOService::poll( sirIOCompletion_t *pCompletions, uint32_t nCapacity ) noexcept
{
	if ( pCompletions == nullptr || nCapacity == 0 ) {
		return 0;
	}
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized ) {
		return 0;
	}
	uint32_t nEmitted = 0;
	for ( uint32_t i = 0; i < m_nRequestCapacity && nEmitted < nCapacity; ++i ) {
		sirIORequestSlot_s& slot = m_pRequests[i];
		if ( slot.eState != SIR_IO_SLOT_TERMINAL ) {
			continue;
		}
		sirIOCompletion_t& completion = pCompletions[nEmitted++];
		completion = { { i, slot.nGeneration }, slot.request.hSource,      slot.request.hBuffer, slot.eResult,
		               slot.nTransferred,       slot.request.nClientToken, slot.nSequence };
		slot.eState = SIR_IO_SLOT_DELIVERED;
	}
	return nEmitted;
}

sirIOResult_t SIRAsyncIOService::acknowledge( sirIORequestHandle_t hRequest ) noexcept
{
	const SIRScopedLock<SIRMutex> guard( m_Mutex );
	if ( !m_bInitialized || !validRequestLocked( hRequest ) ) {
		return SIR_IO_INVALID_HANDLE;
	}
	sirIORequestSlot_s& slot = m_pRequests[hRequest.nSlot];
	if ( slot.eState != SIR_IO_SLOT_DELIVERED ) {
		return SIR_IO_NOT_READY;
	}
	--m_pSources[slot.request.hSource.nSlot].nReferences;
	--m_pBuffers[slot.request.hBuffer.nSlot].nReferences;
	slot.request = {};
	slot.eState = SIR_IO_SLOT_FREE;
	++slot.nGeneration;
	if ( slot.nGeneration == 0 ) {
		++slot.nGeneration;
	}
	return SIR_IO_OK;
}

void SIRAsyncIOService::shutdown( void ) noexcept
{
	{
		const SIRScopedLock<SIRMutex> guard( m_Mutex );
		if ( !m_bInitialized ) {
			return;
		}
		m_bClosing = true;
		m_Ready.notifyAll();
	}
	m_Worker.join();
	for ( uint32_t i = 0; i < m_nSourceCapacity; ++i ) {
		m_pSources[i].file.close();
		m_pSources[i].~sirIOSourceSlot_s();
	}
	for ( uint32_t i = 0; i < m_nBufferCapacity; ++i ) {
		m_pBuffers[i].~sirIOBufferSlot_s();
	}
	for ( uint32_t i = 0; i < m_nRequestCapacity; ++i ) {
		m_pRequests[i].~sirIORequestSlot_s();
	}
	m_pSources = nullptr;
	m_pBuffers = nullptr;
	m_pRequests = nullptr;
	m_nSourceCapacity = 0;
	m_nBufferCapacity = 0;
	m_nRequestCapacity = 0;
	m_bInitialized = false;
}

/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/IO/SIRIOCPReader.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <limits.h>

static constexpr uint32_t SIR_IO_IOCP_READ_BYTES = 1U << 20;

SIRIOCPReader::SIRIOCPReader( void ) noexcept : m_pCompletionPort( nullptr )
{
}

SIRIOCPReader::~SIRIOCPReader( void ) noexcept
{
	shutdown();
}

bool SIRIOCPReader::initialize( void ) noexcept
{
	if ( m_pCompletionPort != nullptr ) {
		return true;
	}
	m_pCompletionPort = CreateIoCompletionPort( INVALID_HANDLE_VALUE, nullptr, 0, 1 );
	return m_pCompletionPort != nullptr;
}

void SIRIOCPReader::shutdown( void ) noexcept
{
	if ( m_pCompletionPort != nullptr ) {
		CloseHandle( ( HANDLE )m_pCompletionPort );
		m_pCompletionPort = nullptr;
	}
}

bool SIRIOCPReader::isAvailable( void ) const noexcept
{
	return m_pCompletionPort != nullptr;
}

bool SIRIOCPReader::submitRead( const SIRPlatformFile& file, uint64_t nOffset, uint8_t *pOutput, uint32_t nBytes,
                                uint32_t *pTransferred ) noexcept
{
	// All SIRPlatformFile Windows handles are opened FILE_FLAG_OVERLAPPED.
	// An inline success also queues one completion unless the file explicitly
	// selects skip-on-success (SIRPlatformFile does not).
	const HANDLE hFile = ( HANDLE )file.m_nNativeHandle;
	if ( CreateIoCompletionPort( hFile, ( HANDLE )m_pCompletionPort, 0, 0 ) != ( HANDLE )m_pCompletionPort ) {
		return false;
	}
	OVERLAPPED pending = {};
	pending.Offset = ( DWORD )nOffset;
	pending.OffsetHigh = ( DWORD )( nOffset >> 32U );
	const BOOL bQueued = ReadFile( hFile, pOutput, nBytes, nullptr, &pending );
	if ( !bQueued && GetLastError() != ERROR_IO_PENDING ) {
		return false;
	}

	DWORD nCompleted = 0;
	ULONG_PTR nKey = 0;
	LPOVERLAPPED pCompleted = nullptr;
	for ( ;; ) {
		const BOOL bSuccess = GetQueuedCompletionStatus( ( HANDLE )m_pCompletionPort, &nCompleted, &nKey, &pCompleted, INFINITE );
		if ( pCompleted != &pending ) {
			// A completion for an unrelated request cannot be treated as ours.
			// Only this worker submits to the port; retain the destination and
			// wait for its exact OVERLAPPED instead of freeing live kernel memory.
			continue;
		}
		if ( !bSuccess ) {
			return false;
		}
		*pTransferred = nCompleted;
		return true;
	}
}

sirFileResult_t SIRIOCPReader::readAt( const SIRPlatformFile& file, uint64_t nOffset, void *pOutput, uint64_t nBytes,
                                       uint64_t *pTransferred ) noexcept
{
	if ( m_pCompletionPort == nullptr ) {
		return file.readAt( nOffset, pOutput, nBytes, pTransferred );
	}
	if ( pTransferred == nullptr || pOutput == nullptr || nBytes == 0 || !file.isOpen() || nOffset > INT64_MAX ||
	     nBytes > INT64_MAX - nOffset ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	uint8_t *pBytes = ( uint8_t * )pOutput;
	while ( *pTransferred < nBytes ) {
		const uint64_t nRemaining = nBytes - *pTransferred;
		const uint32_t nStep = nRemaining > SIR_IO_IOCP_READ_BYTES ? SIR_IO_IOCP_READ_BYTES : ( uint32_t )nRemaining;
		uint32_t nCompleted = 0;
		if ( !submitRead( file, nOffset + *pTransferred, pBytes + *pTransferred, nStep, &nCompleted ) ) {
			return SIR_FILE_IO_ERROR;
		}
		if ( nCompleted == 0 ) {
			return SIR_FILE_END_OF_FILE;
		}
		*pTransferred += nCompleted;
	}
	return SIR_FILE_OK;
}

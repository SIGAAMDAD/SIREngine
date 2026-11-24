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

#include <Core/Platform/Shared/Process/SIRPlatformPipe.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

SIRPlatformPipe::SIRPlatformPipe( void ) noexcept : m_nReadHandle( UINTPTR_MAX ), m_nWriteHandle( UINTPTR_MAX )
{
}

SIRPlatformPipe::SIRPlatformPipe( SIRPlatformPipe&& other ) noexcept
    : m_nReadHandle( other.m_nReadHandle ), m_nWriteHandle( other.m_nWriteHandle )
{
	other.m_nReadHandle = UINTPTR_MAX;
	other.m_nWriteHandle = UINTPTR_MAX;
}

SIRPlatformPipe::~SIRPlatformPipe( void ) noexcept
{
	static_cast<void>( close() );
}

sirPipeResult SIRPlatformPipe::create( void ) noexcept
{
	if ( isReadOpen() || isWriteOpen() ) {
		return sirPipeResult::SIR_PIPE_INVALID_ARGUMENT;
	}
	HANDLE hRead = nullptr;
	HANDLE hWrite = nullptr;
	if ( !CreatePipe( &hRead, &hWrite, nullptr, 0 ) ) {
		return sirPipeResult::SIR_PIPE_IO_ERROR;
	}
	m_nReadHandle = static_cast<uintptr_t>(hRead);
	m_nWriteHandle = static_cast<uintptr_t>(hWrite);
	return sirPipeResult::SIR_PIPE_OK;
}

sirPipeResult SIRPlatformPipe::read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	if ( pTransferred == nullptr ) {
		return sirPipeResult::SIR_PIPE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isReadOpen() ) {
		return sirPipeResult::SIR_PIPE_CLOSED;
	}
	if ( nBytes != 0 && pDestination == nullptr ) {
		return sirPipeResult::SIR_PIPE_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return sirPipeResult::SIR_PIPE_OK;
	}
	const DWORD nStep = static_cast<DWORD>( nBytes < 1024 * 1024 ? nBytes : 1024 * 1024 );
	DWORD nRead = 0;
	if ( !ReadFile( static_cast<HANDLE>(m_nReadHandle), pDestination, nStep, &nRead, nullptr ) ) {
		const DWORD nError = GetLastError();
		return nError == ERROR_BROKEN_PIPE || nError == ERROR_NO_DATA ? sirPipeResult::SIR_PIPE_END_OF_STREAM : sirPipeResult::SIR_PIPE_IO_ERROR;
	}
	*pTransferred = nRead;
	return nRead == 0 ? sirPipeResult::SIR_PIPE_END_OF_STREAM : sirPipeResult::SIR_PIPE_OK;
}

sirPipeResult SIRPlatformPipe::write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	if ( pTransferred == nullptr ) {
		return sirPipeResult::SIR_PIPE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isWriteOpen() ) {
		return sirPipeResult::SIR_PIPE_CLOSED;
	}
	if ( nBytes != 0 && pSource == nullptr ) {
		return sirPipeResult::SIR_PIPE_INVALID_ARGUMENT;
	}
	while ( *pTransferred < nBytes ) {
		const uint64_t nLeft = nBytes - *pTransferred;
		const DWORD nStep = static_cast<DWORD>( nLeft < 64 * 1024 ? nLeft : 64 * 1024 );
		DWORD nWritten = 0;
		if ( !WriteFile( static_cast<HANDLE>(m_nWriteHandle), static_cast<const uint8_t *>(pSource) + *pTransferred, nStep, &nWritten, nullptr ) ) {
			const DWORD nError = GetLastError();
			return nError == ERROR_BROKEN_PIPE || nError == ERROR_NO_DATA ? sirPipeResult::SIR_PIPE_BROKEN : sirPipeResult::SIR_PIPE_IO_ERROR;
		}
		if ( nWritten == 0 ) {
			return sirPipeResult::SIR_PIPE_IO_ERROR;
		}
		*pTransferred += nWritten;
	}
	return sirPipeResult::SIR_PIPE_OK;
}

sirPipeResult SIRPlatformPipe::closeRead( void ) noexcept
{
	if ( !isReadOpen() ) {
		return sirPipeResult::SIR_PIPE_OK;
	}
	HANDLE hRead = static_cast<HANDLE>(m_nReadHandle);
	m_nReadHandle = UINTPTR_MAX;
	return CloseHandle( hRead ) ? sirPipeResult::SIR_PIPE_OK : sirPipeResult::SIR_PIPE_IO_ERROR;
}

sirPipeResult SIRPlatformPipe::closeWrite( void ) noexcept
{
	if ( !isWriteOpen() ) {
		return sirPipeResult::SIR_PIPE_OK;
	}
	HANDLE hWrite = static_cast<HANDLE>(m_nWriteHandle);
	m_nWriteHandle = UINTPTR_MAX;
	return CloseHandle( hWrite ) ? sirPipeResult::SIR_PIPE_OK : sirPipeResult::SIR_PIPE_IO_ERROR;
}

sirPipeResult SIRPlatformPipe::close( void ) noexcept
{
	const sirPipeResult eRead = closeRead();
	const sirPipeResult eWrite = closeWrite();
	return eRead != sirPipeResult::SIR_PIPE_OK ? eRead : eWrite;
}

bool SIRPlatformPipe::isReadOpen( void ) const noexcept
{
	return m_nReadHandle != UINTPTR_MAX;
}

bool SIRPlatformPipe::isWriteOpen( void ) const noexcept
{
	return m_nWriteHandle != UINTPTR_MAX;
}

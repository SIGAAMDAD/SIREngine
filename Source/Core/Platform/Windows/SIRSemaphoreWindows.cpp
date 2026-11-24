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

#include <Core/Platform/SIRSemaphore.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <limits.h>

SIRSemaphore::SIRSemaphore( void ) noexcept : m_pHandle( nullptr )
{
}

SIRSemaphore::~SIRSemaphore( void ) noexcept
{
	static_cast<void>(close());
}

SIRSemaphoreResult SIRSemaphore::create( uint32_t nInitialCount ) noexcept
{
	if ( m_pHandle != nullptr || nInitialCount > INT32_MAX ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_INVALID_ARGUMENT;
	}
	HANDLE hSemaphore = CreateSemaphoreW( nullptr, static_cast<LONG>(nInitialCount), LONG_MAX, nullptr );
	if ( hSemaphore == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_pHandle = hSemaphore;
	return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
}

SIRSemaphoreResult SIRSemaphore::post( void ) noexcept
{
	if ( m_pHandle == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( ReleaseSemaphore( static_cast<HANDLE>(m_pHandle), 1, nullptr ) != FALSE ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	return GetLastError() == ERROR_TOO_MANY_POSTS ? SIRSemaphoreResult::SIR_SEMAPHORE_LIMIT_REACHED : SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

SIRSemaphoreResult SIRSemaphore::wait( void ) noexcept
{
	if ( m_pHandle == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	return WaitForSingleObject( static_cast<HANDLE>(m_pHandle), INFINITE ) == WAIT_OBJECT_0 ? SIRSemaphoreResult::SIR_SEMAPHORE_OK : SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

SIRSemaphoreResult SIRSemaphore::tryWait( void ) noexcept
{
	if ( m_pHandle == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	const DWORD nResult = WaitForSingleObject( static_cast<HANDLE>(m_pHandle), 0 );
	if ( nResult == WAIT_OBJECT_0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	return nResult == WAIT_TIMEOUT ? SIRSemaphoreResult::SIR_SEMAPHORE_WOULD_BLOCK : SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

SIRSemaphoreResult SIRSemaphore::waitFor( uint32_t nMilliseconds ) noexcept
{
	if ( m_pHandle == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( nMilliseconds == 0 ) {
		return tryWait();
	}
	const DWORD nTimeout = nMilliseconds == INFINITE ? INFINITE - 1 : static_cast<DWORD>(nMilliseconds);
	const DWORD nResult = WaitForSingleObject( static_cast<HANDLE>(m_pHandle), nTimeout );
	if ( nResult == WAIT_OBJECT_0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	return nResult == WAIT_TIMEOUT ? SIRSemaphoreResult::SIR_SEMAPHORE_TIMED_OUT : SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

SIRSemaphoreResult SIRSemaphore::close( void ) noexcept
{
	if ( m_pHandle == nullptr ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	if ( CloseHandle( static_cast<HANDLE>(m_pHandle) ) == FALSE ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_pHandle = nullptr;
	return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
}

bool SIRSemaphore::isOpen( void ) const noexcept
{
	return m_pHandle != nullptr;
}

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

#include <errno.h>
#include <limits.h>
#include <time.h>

SIRSemaphore::SIRSemaphore( void ) noexcept : m_Semaphore{}, m_bOpen( false )
{
}

SIRSemaphore::~SIRSemaphore( void ) noexcept
{
	static_cast<void>(close());
}

SIRSemaphoreResult SIRSemaphore::create( uint32_t nInitialCount ) noexcept
{
	if ( m_bOpen || nInitialCount > INT32_MAX ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_INVALID_ARGUMENT;
	}
#if defined( SEM_VALUE_MAX )
	if ( nInitialCount > SEM_VALUE_MAX ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_INVALID_ARGUMENT;
	}
#endif
	if ( sem_init( &m_Semaphore, 0, nInitialCount ) != 0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_bOpen = true;
	return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
}

SIRSemaphoreResult SIRSemaphore::post( void ) noexcept
{
	if ( !m_bOpen ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( sem_post( &m_Semaphore ) == 0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	return errno == EOVERFLOW ? SIRSemaphoreResult::SIR_SEMAPHORE_LIMIT_REACHED : SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

SIRSemaphoreResult SIRSemaphore::wait( void ) noexcept
{
	if ( !m_bOpen ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	for ( ;; ) {
		if ( sem_wait( &m_Semaphore ) == 0 ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno != EINTR ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

SIRSemaphoreResult SIRSemaphore::tryWait( void ) noexcept
{
	if ( !m_bOpen ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	for ( ;; ) {
		if ( sem_trywait( &m_Semaphore ) == 0 ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno == EAGAIN ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_WOULD_BLOCK;
		}
		if ( errno != EINTR ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

SIRSemaphoreResult SIRSemaphore::waitFor( uint32_t nMilliseconds ) noexcept
{
	if ( !m_bOpen ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( nMilliseconds == 0 ) {
		return tryWait();
	}
	struct timespec deadline {};
	if ( clock_gettime( CLOCK_REALTIME, &deadline ) != 0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	const uint64_t nNanoseconds = static_cast<uint64_t>(deadline.tv_nsec) + ( static_cast<uint64_t>(nMilliseconds) % 1000 ) * 1000000ULL;
	deadline.tv_sec += static_cast<time_t>( nMilliseconds / 1000 ) + static_cast<time_t>( nNanoseconds / 1000000000ULL );
	deadline.tv_nsec = static_cast<long>( nNanoseconds % 1000000000ULL );
	for ( ;; ) {
		if ( sem_timedwait( &m_Semaphore, &deadline ) == 0 ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno == ETIMEDOUT ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_TIMED_OUT;
		}
		if ( errno != EINTR ) {
			return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

SIRSemaphoreResult SIRSemaphore::close( void ) noexcept
{
	if ( !m_bOpen ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	if ( sem_destroy( &m_Semaphore ) != 0 ) {
		return SIRSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_bOpen = false;
	return SIRSemaphoreResult::SIR_SEMAPHORE_OK;
}

bool SIRSemaphore::isOpen( void ) const noexcept
{
	return m_bOpen;
}

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

#include <Core/Platform/Shared/Threading/SIRSemaphore.h>

#include <errno.h>
#include <semaphore.h>
#include <new>

static_assert( sizeof( sem_t ) <= sizeof( sirPlatformNativeStorage_t ) );
static_assert( alignof( sem_t ) <= alignof( sirPlatformNativeStorage_t ) );
#include <limits.h>
#include <time.h>

SIRSemaphore::SIRSemaphore( void ) noexcept : m_Semaphore{}, m_bOpen( false )
{
	new ( m_Semaphore.bytes ) sem_t{};
}

SIRSemaphore::~SIRSemaphore( void ) noexcept
{
	static_cast<void>(close());
}

sirSemaphoreResult SIRSemaphore::create( uint32_t nInitialCount ) noexcept
{
	if ( m_bOpen || nInitialCount > INT32_MAX ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_INVALID_ARGUMENT;
	}
#if defined( SEM_VALUE_MAX )
	if ( nInitialCount > SEM_VALUE_MAX ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_INVALID_ARGUMENT;
	}
#endif
	if ( sem_init( reinterpret_cast<sem_t *>( m_Semaphore.bytes ), 0, nInitialCount ) != 0 ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_bOpen = true;
	return sirSemaphoreResult::SIR_SEMAPHORE_OK;
}

sirSemaphoreResult SIRSemaphore::post( void ) noexcept
{
	if ( !m_bOpen ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( sem_post( reinterpret_cast<sem_t *>( m_Semaphore.bytes ) ) == 0 ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	return errno == EOVERFLOW ? sirSemaphoreResult::SIR_SEMAPHORE_LIMIT_REACHED : sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
}

sirSemaphoreResult SIRSemaphore::wait( void ) noexcept
{
	if ( !m_bOpen ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	for ( ;; ) {
		if ( sem_wait( reinterpret_cast<sem_t *>( m_Semaphore.bytes ) ) == 0 ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno != EINTR ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

sirSemaphoreResult SIRSemaphore::tryWait( void ) noexcept
{
	if ( !m_bOpen ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	for ( ;; ) {
		if ( sem_trywait( reinterpret_cast<sem_t *>( m_Semaphore.bytes ) ) == 0 ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno == EAGAIN ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_WOULD_BLOCK;
		}
		if ( errno != EINTR ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

sirSemaphoreResult SIRSemaphore::waitFor( uint32_t nMilliseconds ) noexcept
{
	if ( !m_bOpen ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_CLOSED;
	}
	if ( nMilliseconds == 0 ) {
		return tryWait();
	}
	struct timespec deadline {};
	if ( clock_gettime( CLOCK_REALTIME, &deadline ) != 0 ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	const uint64_t nNanoseconds = static_cast<uint64_t>(deadline.tv_nsec) + ( static_cast<uint64_t>(nMilliseconds) % 1000 ) * 1000000ULL;
	deadline.tv_sec += static_cast<time_t>( nMilliseconds / 1000 ) + static_cast<time_t>( nNanoseconds / 1000000000ULL );
	deadline.tv_nsec = static_cast<long>( nNanoseconds % 1000000000ULL );
	for ( ;; ) {
		if ( sem_timedwait( reinterpret_cast<sem_t *>( m_Semaphore.bytes ), &deadline ) == 0 ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OK;
		}
		if ( errno == ETIMEDOUT ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_TIMED_OUT;
		}
		if ( errno != EINTR ) {
			return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
		}
	}
}

sirSemaphoreResult SIRSemaphore::close( void ) noexcept
{
	if ( !m_bOpen ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_OK;
	}
	if ( sem_destroy( reinterpret_cast<sem_t *>( m_Semaphore.bytes ) ) != 0 ) {
		return sirSemaphoreResult::SIR_SEMAPHORE_OS_ERROR;
	}
	m_bOpen = false;
	return sirSemaphoreResult::SIR_SEMAPHORE_OK;
}

bool SIRSemaphore::isOpen( void ) const noexcept
{
	return m_bOpen;
}

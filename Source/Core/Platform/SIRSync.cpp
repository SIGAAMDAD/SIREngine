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

#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRPlatformUtils.h>

#include <assert.h>
#include <errno.h>

SIRMutex::SIRMutex( void ) noexcept
{
#if defined(_WIN32)
	InitializeSRWLock( &m_hMutex );
#else
	const int nResult = pthread_mutex_init( &m_hMutex, nullptr );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRMutex::~SIRMutex()
{
#if !defined(_WIN32)
	const int nResult = pthread_mutex_destroy( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

void SIRMutex::lock( void ) noexcept
{
#if defined(_WIN32)
	AcquireSRWLockExclusive( &m_hMutex );
#else
	const int nResult = pthread_mutex_lock( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

bool SIRMutex::tryLock( void ) noexcept
{
#if defined(_WIN32)
	return TryAcquireSRWLockExclusive( &m_hMutex ) != FALSE;
#else
	const int nResult = pthread_mutex_trylock( &m_hMutex );
	assert( nResult == 0 || nResult == EBUSY );
	return nResult == 0;
#endif
}

void SIRMutex::unlock( void ) noexcept
{
#if defined(_WIN32)
	ReleaseSRWLockExclusive( &m_hMutex );
#else
	const int nResult = pthread_mutex_unlock( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRRecursiveMutex::SIRRecursiveMutex( void ) noexcept
{
#if defined(_WIN32)
	InitializeCriticalSection( &m_hMutex );
#else
	pthread_mutexattr_t attributes;
	int nResult = pthread_mutexattr_init( &attributes );
	assert( nResult == 0 );
	nResult = pthread_mutexattr_settype( &attributes, PTHREAD_MUTEX_RECURSIVE );
	assert( nResult == 0 );
	nResult = pthread_mutex_init( &m_hMutex, &attributes );
	assert( nResult == 0 );
	nResult = pthread_mutexattr_destroy( &attributes );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRRecursiveMutex::~SIRRecursiveMutex()
{
#if defined(_WIN32)
	DeleteCriticalSection( &m_hMutex );
#else
	const int nResult = pthread_mutex_destroy( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

void SIRRecursiveMutex::lock( void ) noexcept
{
#if defined(_WIN32)
	EnterCriticalSection( &m_hMutex );
#else
	const int nResult = pthread_mutex_lock( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

bool SIRRecursiveMutex::tryLock( void ) noexcept
{
#if defined(_WIN32)
	return TryEnterCriticalSection( &m_hMutex ) != FALSE;
#else
	const int nResult = pthread_mutex_trylock( &m_hMutex );
	assert( nResult == 0 || nResult == EBUSY );
	return nResult == 0;
#endif
}

void SIRRecursiveMutex::unlock( void ) noexcept
{
#if defined(_WIN32)
	LeaveCriticalSection( &m_hMutex );
#else
	const int nResult = pthread_mutex_unlock( &m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRConditionVariable::SIRConditionVariable( void ) noexcept
{
#if defined(_WIN32)
	InitializeConditionVariable( &m_hCondition );
#else
	const int nResult = pthread_cond_init( &m_hCondition, nullptr );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRConditionVariable::~SIRConditionVariable()
{
#if !defined(_WIN32)
	const int nResult = pthread_cond_destroy( &m_hCondition );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

void SIRConditionVariable::wait( SIRMutex &mutex ) noexcept
{
#if defined(_WIN32)
	const BOOL bResult = SleepConditionVariableSRW( &m_hCondition, &mutex.m_hMutex, INFINITE, 0 );
	assert( bResult != FALSE );
	(void)bResult;
#else
	const int nResult = pthread_cond_wait( &m_hCondition, &mutex.m_hMutex );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

void SIRConditionVariable::notifyOne( void ) noexcept
{
#if defined(_WIN32)
	WakeConditionVariable( &m_hCondition );
#else
	const int nResult = pthread_cond_signal( &m_hCondition );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

void SIRConditionVariable::notifyAll( void ) noexcept
{
#if defined(_WIN32)
	WakeAllConditionVariable( &m_hCondition );
#else
	const int nResult = pthread_cond_broadcast( &m_hCondition );
	assert( nResult == 0 );
	(void)nResult;
#endif
}

SIRSpinLock::SIRSpinLock( void ) noexcept
{
	m_bLocked.clear( std::memory_order_relaxed );
}

void SIRSpinLock::lock( void ) noexcept
{
	while ( m_bLocked.test_and_set( std::memory_order_acquire ) ) {
		SIRPlatformUtils::cpuRelax();
	}
}

bool SIRSpinLock::tryLock( void ) noexcept
{
	return !m_bLocked.test_and_set( std::memory_order_acquire );
}

void SIRSpinLock::unlock( void ) noexcept
{
	m_bLocked.clear( std::memory_order_release );
}

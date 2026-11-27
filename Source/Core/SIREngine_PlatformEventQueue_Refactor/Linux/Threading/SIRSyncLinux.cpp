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

#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

#include <assert.h>
#include <stdlib.h>
#include <new>
#if defined( _WIN32 )
#include <Windows.h>
#define SIR_MUTEX_TYPE SRWLOCK
#define SIR_RECURSIVE_TYPE CRITICAL_SECTION
#define SIR_CONDITION_TYPE CONDITION_VARIABLE
#else
#include <pthread.h>
#define SIR_MUTEX_TYPE pthread_mutex_t
#define SIR_RECURSIVE_TYPE pthread_mutex_t
#define SIR_CONDITION_TYPE pthread_cond_t
#endif
static_assert( sizeof( SIR_MUTEX_TYPE ) <= sizeof( sirPlatformNativeStorage_t ) && alignof( SIR_MUTEX_TYPE ) <= alignof( sirPlatformNativeStorage_t ) );
static_assert( sizeof( SIR_RECURSIVE_TYPE ) <= sizeof( sirPlatformNativeStorage_t ) && alignof( SIR_RECURSIVE_TYPE ) <= alignof( sirPlatformNativeStorage_t ) );
static_assert( sizeof( SIR_CONDITION_TYPE ) <= sizeof( sirPlatformNativeStorage_t ) && alignof( SIR_CONDITION_TYPE ) <= alignof( sirPlatformNativeStorage_t ) );
#include <errno.h>

SIRMutex::SIRMutex( void ) noexcept
{
	new ( m_Native.bytes ) SIR_MUTEX_TYPE{};
#if defined( _WIN32 )
	InitializeSRWLock( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_init( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ), nullptr );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

SIRMutex::~SIRMutex()
{
#if !defined( _WIN32 )
	const int nResult = pthread_mutex_destroy( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

void SIRMutex::lock( void ) noexcept
{
#if defined( _WIN32 )
	AcquireSRWLockExclusive( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_lock( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

bool SIRMutex::tryLock( void ) noexcept
{
#if defined( _WIN32 )
	return TryAcquireSRWLockExclusive( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) ) != FALSE;
#else
	const int nResult = pthread_mutex_trylock( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
	assert( nResult == 0 || nResult == EBUSY );
	return nResult == 0;
#endif
}

void SIRMutex::unlock( void ) noexcept
{
#if defined( _WIN32 )
	ReleaseSRWLockExclusive( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_unlock( reinterpret_cast<SIR_MUTEX_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

SIRRecursiveMutex::SIRRecursiveMutex( void ) noexcept
{
	new ( m_Native.bytes ) SIR_RECURSIVE_TYPE{};
#if defined( _WIN32 )
	InitializeCriticalSection( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
#else
	pthread_mutexattr_t attributes;
	int nResult = pthread_mutexattr_init( &attributes );
	if ( nResult != 0 ) { abort(); }
	nResult = pthread_mutexattr_settype( &attributes, PTHREAD_MUTEX_RECURSIVE );
	if ( nResult != 0 ) { abort(); }
	nResult = pthread_mutex_init( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ), &attributes );
	if ( nResult != 0 ) { abort(); }
	nResult = pthread_mutexattr_destroy( &attributes );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

SIRRecursiveMutex::~SIRRecursiveMutex()
{
#if defined( _WIN32 )
	DeleteCriticalSection( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_destroy( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

void SIRRecursiveMutex::lock( void ) noexcept
{
#if defined( _WIN32 )
	EnterCriticalSection( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_lock( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

bool SIRRecursiveMutex::tryLock( void ) noexcept
{
#if defined( _WIN32 )
	return TryEnterCriticalSection( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) ) != FALSE;
#else
	const int nResult = pthread_mutex_trylock( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
	assert( nResult == 0 || nResult == EBUSY );
	return nResult == 0;
#endif
}

void SIRRecursiveMutex::unlock( void ) noexcept
{
#if defined( _WIN32 )
	LeaveCriticalSection( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_mutex_unlock( reinterpret_cast<SIR_RECURSIVE_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

SIRConditionVariable::SIRConditionVariable( void ) noexcept
{
	new ( m_Native.bytes ) SIR_CONDITION_TYPE{};
#if defined( _WIN32 )
	InitializeConditionVariable( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_cond_init( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ), nullptr );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

SIRConditionVariable::~SIRConditionVariable()
{
#if !defined( _WIN32 )
	const int nResult = pthread_cond_destroy( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

void SIRConditionVariable::wait( SIRMutex& mutex ) noexcept
{
#if defined( _WIN32 )
	const BOOL bResult = SleepConditionVariableSRW( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ), reinterpret_cast<SIR_MUTEX_TYPE *>( mutex.m_Native.bytes ), INFINITE, 0 );
	assert( bResult != FALSE );
	static_cast<void>(bResult);
#else
	const int nResult = pthread_cond_wait( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ), reinterpret_cast<SIR_MUTEX_TYPE *>( mutex.m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

void SIRConditionVariable::notifyOne( void ) noexcept
{
#if defined( _WIN32 )
	WakeConditionVariable( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_cond_signal( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}

void SIRConditionVariable::notifyAll( void ) noexcept
{
#if defined( _WIN32 )
	WakeAllConditionVariable( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
#else
	const int nResult = pthread_cond_broadcast( reinterpret_cast<SIR_CONDITION_TYPE *>( m_Native.bytes ) );
	if ( nResult != 0 ) { abort(); }
	static_cast<void>(nResult);
#endif
}


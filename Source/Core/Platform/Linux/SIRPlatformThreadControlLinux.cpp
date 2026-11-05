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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <Core/Platform/SIRPlatformThreadControl.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

static sirThreadControlResult_t sirThreadResultFromErrno( int nError ) noexcept
{
	if ( nError == 0 ) {
		return SIR_THREAD_CONTROL_OK;
	}
	if ( nError == EPERM || nError == EACCES ) {
		return SIR_THREAD_CONTROL_ACCESS_DENIED;
	}
	return nError == EINVAL ? SIR_THREAD_CONTROL_INVALID_ARGUMENT : SIR_THREAD_CONTROL_OS_ERROR;
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentName( const char *pszName ) noexcept
{
	if ( pszName == nullptr || pszName[0] == '\0' ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	if ( strlen( pszName ) > 15 ) {
		return SIR_THREAD_CONTROL_UNSUPPORTED;
	}
	return sirThreadResultFromErrno( pthread_setname_np( pthread_self(), pszName ) );
}

sirThreadControlResult_t SIRPlatformThreadControl::getCurrentName( char *pszName, uint32_t nCapacity ) noexcept
{
	if ( pszName == nullptr || nCapacity < 16 ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	return sirThreadResultFromErrno( pthread_getname_np( pthread_self(), pszName, nCapacity ) );
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentAffinity( const sirThreadAffinity_t& rAffinity ) noexcept
{
	if ( rAffinity.nMask == 0 ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	if ( rAffinity.nGroup != 0 ) {
		return SIR_THREAD_CONTROL_UNSUPPORTED;
	}
	cpu_set_t cpus;
	CPU_ZERO( &cpus );
	for ( uint32_t i = 0; i < 64; ++i ) {
		if ( ( rAffinity.nMask & ( 1ULL << i ) ) != 0 ) {
			CPU_SET( i, &cpus );
		}
	}
	return sirThreadResultFromErrno( pthread_setaffinity_np( pthread_self(), sizeof( cpus ), &cpus ) );
}

sirThreadControlResult_t SIRPlatformThreadControl::getCurrentAffinity( sirThreadAffinity_t *pAffinity ) noexcept
{
	if ( pAffinity == nullptr ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	cpu_set_t cpus;
	CPU_ZERO( &cpus );
	const int nResult = pthread_getaffinity_np( pthread_self(), sizeof( cpus ), &cpus );
	if ( nResult != 0 ) {
		return sirThreadResultFromErrno( nResult );
	}
	for ( uint32_t i = 64; i < ( uint32_t )CPU_SETSIZE; ++i ) {
		if ( CPU_ISSET( i, &cpus ) ) {
			return SIR_THREAD_CONTROL_UNSUPPORTED;
		}
	}
	sirThreadAffinity_t affinity{};
	for ( uint32_t i = 0; i < 64; ++i ) {
		if ( CPU_ISSET( i, &cpus ) ) {
			affinity.nMask |= 1ULL << i;
		}
	}
	affinity.nGroup = 0;
	*pAffinity = affinity;
	return SIR_THREAD_CONTROL_OK;
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentPriority( sirThreadPriority_t ePriority ) noexcept
{
	int nNice = 0;
	switch ( ePriority ) {
	case SIR_THREAD_PRIORITY_BACKGROUND:
		nNice = 10;
		break;
	case SIR_THREAD_PRIORITY_NORMAL:
		nNice = 0;
		break;
	case SIR_THREAD_PRIORITY_HIGH:
		nNice = -5;
		break;
	default:
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	const id_t nThreadId = ( id_t )syscall( SYS_gettid );
	return setpriority( PRIO_PROCESS, nThreadId, nNice ) == 0 ? SIR_THREAD_CONTROL_OK : sirThreadResultFromErrno( errno );
}

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

#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>

#include <errno.h>
#include <sched.h>
#include <unistd.h>

uint32_t SIRCPUInfo::getLogicalProcessorCount( void ) noexcept
{
	const long nCount = sysconf( _SC_NPROCESSORS_ONLN );
	return nCount > 0 ? static_cast<uint32_t>(nCount) : 1;
}

void SIRThread::cpuRelax( void ) noexcept
{
#if defined( __x86_64__ ) || defined( __i386__ )
	__builtin_ia32_pause();
#elif defined( __aarch64__ )
	__asm__ volatile( "yield" );
#endif
}

void SIRThread::yieldThread( void ) noexcept
{
	sched_yield();
}

void SIRThread::sleepThread( uint32_t nMilliseconds ) noexcept
{
	timespec request = { static_cast<time_t>( nMilliseconds / 1000 ), static_cast<long>( nMilliseconds % 1000 ) * 1000000L };
	while ( nanosleep( &request, &request ) != 0 && errno == EINTR ) {
	}
}

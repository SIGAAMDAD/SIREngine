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

#include <Core/Platform/SIRPlatformProcess.h>
#include <Core/Platform/SIRPlatformTime.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define SIR_PLATFORM_FAULT_CHECK( condition )                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Platform fault test failed %s:%d: %s\n", __FILE__, __LINE__, #condition );                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static bool s_bFailClock = false;
static bool s_bFailProcessorCount = false;
static bool s_bInterruptSleep = false;
static uint32_t s_nSleepCalls = 0;

extern "C" int __real_clock_gettime( clockid_t eClock, struct timespec *pTime );
extern "C" long __real_sysconf( int nName );
extern "C" int __real_nanosleep( const struct timespec *pRequest, struct timespec *pRemaining );

extern "C" int __wrap_clock_gettime( clockid_t eClock, struct timespec *pTime )
{
	if ( s_bFailClock && eClock == CLOCK_MONOTONIC ) {
		s_bFailClock = false;
		errno = EINVAL;
		return -1;
	}
	return __real_clock_gettime( eClock, pTime );
}

extern "C" long __wrap_sysconf( int nName )
{
	if ( s_bFailProcessorCount && nName == _SC_NPROCESSORS_ONLN ) {
		s_bFailProcessorCount = false;
		errno = EINVAL;
		return -1;
	}
	return __real_sysconf( nName );
}

extern "C" int __wrap_nanosleep( const struct timespec *pRequest, struct timespec *pRemaining )
{
	++s_nSleepCalls;
	if ( s_bInterruptSleep ) {
		s_bInterruptSleep = false;
		if ( pRemaining != nullptr ) {
			*pRemaining = *pRequest;
		}
		errno = EINTR;
		return -1;
	}
	return __real_nanosleep( pRequest, pRemaining );
}

int main( void )
{
	s_bFailClock = true;
	SIR_PLATFORM_FAULT_CHECK( SIRPlatformTime::getMonotonicNanoseconds() == 0 );
	SIR_PLATFORM_FAULT_CHECK( !s_bFailClock );
	SIR_PLATFORM_FAULT_CHECK( SIRPlatformTime::getMonotonicNanoseconds() != 0 );

	s_bFailProcessorCount = true;
	SIR_PLATFORM_FAULT_CHECK( SIRPlatformProcess::getLogicalProcessorCount() == 1 );
	SIR_PLATFORM_FAULT_CHECK( !s_bFailProcessorCount );
	SIR_PLATFORM_FAULT_CHECK( SIRPlatformProcess::getLogicalProcessorCount() > 0 );

	s_bInterruptSleep = true;
	s_nSleepCalls = 0;
	SIRPlatformProcess::sleepThread( 0 );
	SIR_PLATFORM_FAULT_CHECK( !s_bInterruptSleep && s_nSleepCalls == 2 );
	SIRPlatformProcess::cpuRelax();
	SIRPlatformProcess::yieldThread();
	puts( "PASS: platform clock and process syscall faults" );
	return 0;
}

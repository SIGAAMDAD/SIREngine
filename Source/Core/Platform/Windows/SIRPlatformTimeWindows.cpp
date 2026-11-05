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

#include <Core/Platform/SIRPlatformTime.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

uint64_t SIRPlatformTime::getMonotonicNanoseconds( void ) noexcept
{
	// Cache the QPC frequency once and use integer conversion to nanoseconds.
	static const uint64_t s_nFrequency = []() {
		LARGE_INTEGER frequency = {};
		return QueryPerformanceFrequency( &frequency ) != FALSE && frequency.QuadPart > 0 ? ( uint64_t )frequency.QuadPart : 0;
	}();
	LARGE_INTEGER ticks = {};
	if ( s_nFrequency == 0 || QueryPerformanceCounter( &ticks ) == FALSE || ticks.QuadPart < 0 ) {
		return 0;
	}
	const uint64_t nTicks = ( uint64_t )ticks.QuadPart;
	return ( nTicks / s_nFrequency ) * UINT64_C( 1000000000 ) + ( nTicks % s_nFrequency ) * UINT64_C( 1000000000 ) / s_nFrequency;
}

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

#include <time.h>

uint64_t SIRPlatformTime::getMonotonicNanoseconds( void ) noexcept
{
	timespec time = {};
	if ( clock_gettime( CLOCK_MONOTONIC, &time ) != 0 ) {
		return 0;
	}
	return (uint64_t)time.tv_sec * UINT64_C(1000000000) + (uint64_t)time.tv_nsec;
}

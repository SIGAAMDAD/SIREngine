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

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

uint32_t SIRPlatformProcess::getLogicalProcessorCount( void ) noexcept
{
	const DWORD nCount = GetActiveProcessorCount( ALL_PROCESSOR_GROUPS );
	return nCount > 0 ? (uint32_t)nCount : 1;
}

void SIRPlatformProcess::cpuRelax( void ) noexcept
{
	YieldProcessor();
}
void SIRPlatformProcess::yieldThread( void ) noexcept
{
	SwitchToThread();
}
void SIRPlatformProcess::sleepThread( uint32_t nMilliseconds ) noexcept
{
	Sleep( nMilliseconds );
}

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

#include <Core/Platform/SIRSystemInfo.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <string.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

bool SIRSystemInfo::query( sirSystemInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return false;
	}
	struct utsname os = {};
	if ( uname( &os ) != 0 ) {
		return false;
	}
	sirSystemInfo_t info = {};
	info.eOS = SIR_OS_LINUX;
#if defined(__x86_64__)
	info.eArchitecture = SIR_ARCH_X64;
#elif defined(__aarch64__)
	info.eArchitecture = SIR_ARCH_ARM64;
#endif
	memcpy( info.szOSName, "Linux", 6 );
	strncpy( info.szOSRelease, os.release, sizeof( info.szOSRelease ) - 1 );
	if ( gethostname( info.szHostName, sizeof( info.szHostName ) ) != 0 ) {
		return false;
	}
	info.szHostName[sizeof( info.szHostName ) - 1] = '\0';
	info.nProcessId = (uint64_t)getpid();
	info.nPageSize = (uint32_t)SIRVirtualMemory::getPageSize();
	info.nAllocationGranularity = (uint32_t)SIRVirtualMemory::getAllocationGranularity();
	struct sysinfo system = {};
	if ( sysinfo( &system ) == 0 && system.uptime > 0 ) {
		info.nUptimeSeconds = (uint64_t)system.uptime;
	}
	if ( info.nPageSize == 0 ) {
		return false;
	}
	*pInfo = info;
	return true;
}

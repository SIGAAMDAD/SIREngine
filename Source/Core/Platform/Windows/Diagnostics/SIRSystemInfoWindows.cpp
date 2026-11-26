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

#include <Core/Platform/Shared/Diagnostics/SIRSystemInfo.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winternl.h>
#include <string.h>

bool SIRSystemInfo::query( sirSystemInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return false;
	}
	sirSystemInfo_t info {};
	info.eOS = sirOperatingSystem::SIR_OS_WINDOWS;
#if defined( _M_X64 )
	info.eArchitecture = sirCPUArchitecture::SIR_ARCH_X64;
#elif defined( _M_ARM64 )
	info.eArchitecture = sirCPUArchitecture::SIR_ARCH_ARM64;
#endif
	memcpy( info.szOSName, "Windows", 8 );
	info.nProcessId = static_cast<uint64_t>(GetCurrentProcessId());
	info.nUptimeSeconds = static_cast<uint64_t>(GetTickCount64()) / 1000U;
	info.nPageSize = static_cast<uint32_t>(SIRVirtualMemory)::getPageSize();
	info.nAllocationGranularity = static_cast<uint32_t>(SIRVirtualMemory)::getAllocationGranularity();
	WCHAR szHost[256];
	memset( szHost, 0, sizeof( szHost ) );
	DWORD nHostLength = static_cast<DWORD>(  sizeof( szHost ) / sizeof( szHost[0] )  );
	if ( !GetComputerNameW( szHost, &nHostLength ) ) {
		return false;
	}
	if ( !WideCharToMultiByte( CP_UTF8, 0, szHost, -1, info.szHostName, static_cast<int>(sizeof( info.szHostName ), nullptr, nullptr ) )) {
		return false;
	}
	HMODULE pNTDLL = GetModuleHandleW( L"ntdll.dll" );
	if ( pNTDLL != nullptr ) {
		typedef LONG( WINAPI * sirQueryVersion_t )( PRTL_OSVERSIONINFOW );
		sirQueryVersion_t pVersion = static_cast<sirQueryVersion_t>(GetProcAddress( pNTDLL, "RtlGetVersion" ));
		if ( pVersion != nullptr ) {
			RTL_OSVERSIONINFOW version {};
			version.dwOSVersionInfoSize = sizeof( version );
			if ( pVersion( &version ) == 0 ) {
				info.nOSMajor = static_cast<uint32_t>(version.dwMajorVersion);
				info.nOSMinor = static_cast<uint32_t>(version.dwMinorVersion);
				info.nOSBuild = static_cast<uint32_t>(version.dwBuildNumber);
			}
		}
	}
	if ( info.nPageSize == 0 ) {
		return false;
	}
	*pInfo = info;
	return true;
}

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

#include <Core/Platform/SIRVirtualMemory.h>

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static const SYSTEM_INFO *getSystemMemoryInfo( void ) noexcept
{
	static const SYSTEM_INFO s_Info = []() {
		SYSTEM_INFO info = {};
		GetSystemInfo( &info );
		return info;
	}();
	return &s_Info;
}

static bool validPageRange( const void *pAddress, size_t nBytes ) noexcept
{
	const size_t nPageSize = SIRVirtualMemory::getPageSize();
	return pAddress != nullptr && nPageSize != 0 && nBytes != 0 &&
		( (uintptr_t)pAddress % nPageSize ) == 0 && ( nBytes % nPageSize ) == 0;
}

size_t SIRVirtualMemory::getPageSize( void ) noexcept
{
	return (size_t)getSystemMemoryInfo()->dwPageSize;
}
size_t SIRVirtualMemory::getAllocationGranularity( void ) noexcept
{
	return (size_t)getSystemMemoryInfo()->dwAllocationGranularity;
}

void *SIRVirtualMemory::reserve( size_t nBytes ) noexcept
{
	const size_t nPageSize = getPageSize();
	if ( nPageSize == 0 || nBytes == 0 || ( nBytes % nPageSize ) != 0 ) {
		return nullptr;
	}
	return VirtualAlloc( nullptr, nBytes, MEM_RESERVE, PAGE_NOACCESS );
}

bool SIRVirtualMemory::commit( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	return VirtualAlloc( pAddress, nBytes, MEM_COMMIT, PAGE_READWRITE ) == pAddress;
}

bool SIRVirtualMemory::decommit( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	return VirtualFree( pAddress, nBytes, MEM_DECOMMIT ) != FALSE;
}

bool SIRVirtualMemory::release( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	// Windows releases the complete reservation; MEM_RELEASE requires a zero size.
	return VirtualFree( pAddress, 0, MEM_RELEASE ) != FALSE;
}

bool SIRVirtualMemory::protect( void *pAddress, size_t nBytes, sirMemoryProtection_t eProtection ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	DWORD nFlags = PAGE_NOACCESS;
	switch ( eProtection ) {
		case SIR_MEMORY_NO_ACCESS: nFlags = PAGE_NOACCESS; break;
		case SIR_MEMORY_READ_ONLY: nFlags = PAGE_READONLY; break;
		case SIR_MEMORY_READ_WRITE: nFlags = PAGE_READWRITE; break;
		case SIR_MEMORY_READ_EXECUTE: nFlags = PAGE_EXECUTE_READ; break;
		default: return false;
	}
	DWORD nPrevious = 0;
	return VirtualProtect( pAddress, nBytes, nFlags, &nPrevious ) != FALSE;
}

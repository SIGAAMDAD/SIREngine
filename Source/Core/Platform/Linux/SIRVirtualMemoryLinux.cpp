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

#include <sys/mman.h>
#include <unistd.h>

static bool validPageRange( const void *pAddress, size_t nBytes ) noexcept
{
	const size_t nPageSize = SIRVirtualMemory::getPageSize();
	return pAddress != nullptr && nPageSize != 0 && nBytes != 0 && ( ( uintptr_t )pAddress % nPageSize ) == 0 &&
	       ( nBytes % nPageSize ) == 0;
}

size_t SIRVirtualMemory::getPageSize( void ) noexcept
{
	static const long s_nPageSize = sysconf( _SC_PAGESIZE );
	return s_nPageSize > 0 ? ( size_t )s_nPageSize : 0;
}

size_t SIRVirtualMemory::getAllocationGranularity( void ) noexcept
{
	return getPageSize();
}

void *SIRVirtualMemory::reserve( size_t nBytes ) noexcept
{
	const size_t nPageSize = getPageSize();
	if ( nPageSize == 0 || nBytes == 0 || ( nBytes % nPageSize ) != 0 ) {
		return nullptr;
	}
	void *pAddress = mmap( nullptr, nBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
	return pAddress == MAP_FAILED ? nullptr : pAddress;
}

bool SIRVirtualMemory::commit( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	return mprotect( pAddress, nBytes, PROT_READ | PROT_WRITE ) == 0;
}

bool SIRVirtualMemory::decommit( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	if ( mprotect( pAddress, nBytes, PROT_NONE ) != 0 ) {
		return false;
	}
	// A failed discard must restore accessibility: the memory owner will
	// retain these pages as committed when this call reports failure.
	if ( madvise( pAddress, nBytes, MADV_DONTNEED ) != 0 ) {
		mprotect( pAddress, nBytes, PROT_READ | PROT_WRITE );
		return false;
	}
	return true;
}

bool SIRVirtualMemory::release( void *pAddress, size_t nBytes ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	return munmap( pAddress, nBytes ) == 0;
}

bool SIRVirtualMemory::protect( void *pAddress, size_t nBytes, sirMemoryProtection_t eProtection ) noexcept
{
	if ( !validPageRange( pAddress, nBytes ) ) {
		return false;
	}
	int nFlags = PROT_NONE;
	switch ( eProtection ) {
	case SIR_MEMORY_NO_ACCESS:
		nFlags = PROT_NONE;
		break;
	case SIR_MEMORY_READ_ONLY:
		nFlags = PROT_READ;
		break;
	case SIR_MEMORY_READ_WRITE:
		nFlags = PROT_READ | PROT_WRITE;
		break;
	case SIR_MEMORY_READ_EXECUTE:
		nFlags = PROT_READ | PROT_EXEC;
		break;
	default:
		return false;
	}
	return mprotect( pAddress, nBytes, nFlags ) == 0;
}

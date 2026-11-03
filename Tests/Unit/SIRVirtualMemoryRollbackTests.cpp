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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

extern "C" int __wrap_madvise( void *pAddress, size_t nBytes, int nAdvice ) noexcept
{
	( void )pAddress;
	( void )nBytes;
	( void )nAdvice;
	errno = EIO;
	return -1;
}

int main( void )
{
	const size_t nPage = SIRVirtualMemory::getPageSize();
	void *pAddress = SIRVirtualMemory::reserve( nPage );
	if ( pAddress == nullptr || !SIRVirtualMemory::commit( pAddress, nPage ) ) {
		return 1;
	}

	volatile uint8_t *pBytes = ( volatile uint8_t * )pAddress;
	pBytes[0] = 0x5A;
	if ( SIRVirtualMemory::decommit( pAddress, nPage ) || pBytes[0] != 0x5A ) {
		fputs( "failed decommit damaged committed page\n", stderr );
		return 1;
	}
	const bool bReleased = SIRVirtualMemory::release( pAddress, nPage );
	return bReleased ? 0 : 1;
}

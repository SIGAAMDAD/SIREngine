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

void *SIRVirtualMemory::allocatePages( size_t nBytes ) noexcept
{
	void *pAddress = reserve( nBytes );
	if ( pAddress == nullptr ) {
		return nullptr;
	}
	if ( !commit( pAddress, nBytes ) ) {
		release( pAddress, nBytes );
		return nullptr;
	}
	return pAddress;
}

bool SIRVirtualMemory::freePages( void *pAddress, size_t nBytes ) noexcept
{
	return release( pAddress, nBytes );
}

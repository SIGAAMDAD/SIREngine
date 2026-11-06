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

#include <Resources/SIRResourceMemoryAdapter.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirAdapterRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "resource adapter test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirAdapterRequire( ( condition ), __LINE__ )

int main( void )
{
	SIRResourceMemoryAdapter adapter;
	REQUIRE( !adapter.bindDomain( ( sirMemoryDomain_t )SIR_MEMORY_DOMAIN_COUNT, nullptr ) );
	REQUIRE( !adapter.bindDomain( SIR_MEMORY_DOMAIN_CORE, nullptr ) );
	SIRResourceAllocator callbacks = adapter.getAllocator();
	REQUIRE( callbacks.m_pContext == &adapter );
	REQUIRE( callbacks.m_pfnAllocate != nullptr && callbacks.m_pfnFree != nullptr );
	REQUIRE( callbacks.m_pfnAllocate( nullptr, 16, 8, SIR_MEMORY_DOMAIN_CORE ) == nullptr );
	REQUIRE( callbacks.m_pfnAllocate( callbacks.m_pContext, 0, 8, SIR_MEMORY_DOMAIN_CORE ) == nullptr );
	REQUIRE( callbacks.m_pfnAllocate( callbacks.m_pContext, 8, 0, SIR_MEMORY_DOMAIN_CORE ) == nullptr );
	REQUIRE( callbacks.m_pfnAllocate( callbacks.m_pContext, 8, 3, SIR_MEMORY_DOMAIN_CORE ) == nullptr );
	REQUIRE( callbacks.m_pfnAllocate( callbacks.m_pContext, 8, 8, SIR_MEMORY_DOMAIN_COUNT ) == nullptr );
	REQUIRE( callbacks.m_pfnAllocate( callbacks.m_pContext, 8, 8, SIR_MEMORY_DOMAIN_CORE ) == nullptr );
	callbacks.m_pfnFree( nullptr, nullptr, 0, 0, SIR_MEMORY_DOMAIN_COUNT );
	callbacks.m_pfnFree( callbacks.m_pContext, nullptr, 8, 8, SIR_MEMORY_DOMAIN_CORE );
	return 0;
}

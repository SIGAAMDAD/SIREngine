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

#include <Scripting/Core/SIRScriptMemoryRouter.h>
#include <Core/Memory/SIRAllocationRouter.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdio.h>
#include <string.h>

#define SIR_TEST_CHECK( condition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static bool testInvalidRouterInputs( void ) noexcept
{
	const sirScriptPayloadAllocatorAPI_t unavailable = SIRScriptMemoryRouter::makeAPI( nullptr, 128, 16 );
	SIR_TEST_CHECK( unavailable.nABIRevision == 0 && unavailable.pAllocate == nullptr && unavailable.pRelease == nullptr );

	SIRAllocationRouter router;
	const sirScriptPayloadAllocatorAPI_t api = SIRScriptMemoryRouter::makeAPI( &router, 128, 16 );
	SIR_TEST_CHECK( api.nABIRevision == SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION );
	SIR_TEST_CHECK( api.pUser == &router && api.nMaximumBytes == 128 && api.nMaximumAlignment == 16 );
	SIR_TEST_CHECK( api.pAllocate != nullptr && api.pRelease != nullptr );

	sirScriptPayloadAllocation_t allocation = {};
	SIR_TEST_CHECK( api.pAllocate( nullptr, 64, 16, &allocation ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( api.pAllocate( &router, 64, 16, nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( api.pAllocate( &router, 0, 16, &allocation ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( api.pAllocate( &router, 64, 16, &allocation ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( allocation.pData == nullptr && allocation.nToken == 0 );
	api.pRelease( nullptr, allocation );
	api.pRelease( &router, allocation );
	return true;
}

static bool testRoutedPayloadLease( void ) noexcept
{
	const uint64_t nPageSize = SIRVirtualMemory::getPageSize();
	SIR_TEST_CHECK( nPageSize != 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t memoryConfig = { nPageSize * 8192, nPageSize * 4096, nPageSize * 256, 16 };
	SIR_TEST_CHECK( memory.init( memoryConfig ) );
	SIRMemorySegment *pMedium = memory.createSegment( "script-payload", nPageSize * 512 );
	SIR_TEST_CHECK( pMedium != nullptr );

	SIRFreeListAllocator medium;
	SIR_TEST_CHECK( medium.init( pMedium ) );
	SIRSizeClassAllocator unusedClasses;
	SIRBuddyAllocator unusedLarge;
	const sirAllocationRouterConfig_t config = { &unusedClasses, nullptr, &medium, nullptr, &unusedLarge, nullptr,
	                                             nullptr,        1,       4096,    false,   false };
	SIRAllocationRouter router;
	SIR_TEST_CHECK( router.init( config ) );
	const sirScriptPayloadAllocatorAPI_t api = SIRScriptMemoryRouter::makeAPI( &router, 512, 64 );

	sirScriptPayloadAllocation_t allocation = {};
	SIR_TEST_CHECK( api.pAllocate( api.pUser, 128, 16, &allocation ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( allocation.pData != nullptr );
	SIR_TEST_CHECK( ( ( uintptr_t )allocation.pData & 15 ) == 0 );
	SIR_TEST_CHECK( ( uint32_t )allocation.nToken == 128 );
	SIR_TEST_CHECK( ( uint32_t )( allocation.nToken >> 32 ) == SIR_ROUTE_FREE_LIST );
	SIR_TEST_CHECK( medium.getAllocationCount() == 1 );
	memset( allocation.pData, 0x5a, 128 );
	SIR_TEST_CHECK( ( ( const uint8_t * )allocation.pData )[127] == 0x5a );
	api.pRelease( api.pUser, allocation );
	SIR_TEST_CHECK( medium.getAllocationCount() == 0 );
	SIR_TEST_CHECK( router.getRoutedCount( SIR_ROUTE_FREE_LIST ) == 1 );

	SIR_TEST_CHECK( api.pAllocate( api.pUser, 128, 3, &allocation ) == SIR_SCRIPT_CAPACITY_EXCEEDED );
	SIR_TEST_CHECK( allocation.pData == nullptr && allocation.nToken == 0 );
	SIR_TEST_CHECK( medium.shutdown() );
	memory.shutdown();
	return true;
}

int main( void )
{
	if ( !testInvalidRouterInputs() || !testRoutedPayloadLease() ) {
		return 1;
	}
	puts( "SIRScriptMemoryRouter tests PASS" );
	return 0;
}

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

#include <Core/Memory/Allocators/SIRVirtualStackAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirStackRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "virtual stack coverage test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirStackRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage > 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 256, nPage * 96, nPage * 16, 8 };
	REQUIRE( memory.init( config ) );
	SIRMemorySegment *pEmpty = memory.createSegment( "stack.empty", nPage * 16 );
	SIRMemorySegment *pExcess = memory.createSegment( "stack.excess", nPage * 16 );
	SIRMemorySegment *pShutdown = memory.createSegment( "stack.shutdown", nPage * 16 );
	SIRMemorySegment *pTiny = memory.createSegment( "stack.tiny", nPage );
	REQUIRE( pEmpty != nullptr && pExcess != nullptr && pShutdown != nullptr && pTiny != nullptr );

	SIRVirtualStackAllocator stack;
	REQUIRE( !stack.init( nullptr, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	REQUIRE( !stack.init( pTiny, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	REQUIRE( !stack.init( pEmpty, ( sirStackDecommitMode_t )255 ) );
	REQUIRE( stack.init( pEmpty, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	REQUIRE( !stack.init( pEmpty, SIR_STACK_DECOMMIT_ON_EMPTY ) );
	REQUIRE( stack.getCapacity() == nPage * 15 );
	REQUIRE( stack.reset() );
	REQUIRE( stack.precommit( nPage ) );
	REQUIRE( stack.alloc( 32, 3 ) == nullptr );
	REQUIRE( stack.alloc( UINT64_MAX, 16 ) == nullptr );
	void *pFirst = stack.alloc( 64, 16 );
	REQUIRE( pFirst != nullptr && stack.getUsedBytes() >= 64 );
	{
		auto bookmark = stack.createScopedBookmark();
		REQUIRE( stack.alloc( 512, 32 ) != nullptr );
		REQUIRE( !stack.reset() );
		REQUIRE( !stack.shutdown() );
	}
	REQUIRE( stack.getUsedBytes() >= 64 );
	REQUIRE( stack.reset() );
	REQUIRE( stack.getUsedBytes() == 0 && stack.getCommittedBytes() == 0 );
	{
		auto bookmark = stack.createScopedBookmark();
		REQUIRE( stack.alloc( 96, 16 ) != nullptr );
	}
	REQUIRE( stack.getUsedBytes() == 0 && stack.getCommittedBytes() == 0 );
	REQUIRE( stack.shutdown() );

	SIRVirtualStackAllocator excess;
	REQUIRE( excess.init( pExcess, SIR_STACK_DECOMMIT_EXCESS_ON_EMPTY ) );
	REQUIRE( excess.precommit( nPage * 6 ) );
	REQUIRE( excess.getCommittedBytes() >= nPage * 6 );
	REQUIRE( excess.alloc( 32, 16 ) != nullptr );
	REQUIRE( excess.reset() );
	REQUIRE( excess.getCommittedBytes() <= nPage );
	REQUIRE( excess.reset() );
	REQUIRE( excess.getCommittedBytes() == 0 );
	REQUIRE( excess.shutdown() );

	SIRVirtualStackAllocator retained;
	REQUIRE( retained.init( pShutdown, SIR_STACK_DECOMMIT_ON_SHUTDOWN ) );
	REQUIRE( retained.precommit( nPage * 3 ) );
	REQUIRE( retained.alloc( 96, 16 ) != nullptr );
	REQUIRE( retained.reset() );
	REQUIRE( retained.getUsedBytes() == 0 );
	REQUIRE( retained.getCommittedBytes() >= nPage * 3 );
	REQUIRE( retained.shutdown() );
	memory.shutdown();
	return 0;
}

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

#include <Core/Memory/SIRMemoryPressureCoordinator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirPressureRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "pressure coverage test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirPressureRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage > 0 );
	SIRMemoryPressureCoordinator pressure;
	REQUIRE( !pressure.requestReclaim( nPage ) );
	REQUIRE( !pressure.cancelPending() );
	REQUIRE( !pressure.shutdown() );
	REQUIRE( pressure.serviceOwner( 0 ) == 0 );
	REQUIRE( pressure.findNextPendingOwner() == UINT32_MAX );
	REQUIRE( !pressure.init( nullptr, 1 ) );

	SIRMemoryManager memory;
	REQUIRE( !pressure.init( &memory, 1 ) );
	const sirMemoryConfig_t config = { nPage * 256, nPage * 128, nPage * 16, 8 };
	REQUIRE( memory.init( config ) );
	REQUIRE( !pressure.init( &memory, 0 ) );
	REQUIRE( pressure.init( &memory, 2 ) );
	REQUIRE( !pressure.init( &memory, 1 ) );

	SIRMemorySegment *pSegmentA = memory.createSegment( "pressure.a", nPage * 32 );
	SIRMemorySegment *pSegmentB = memory.createSegment( "pressure.b", nPage * 32 );
	REQUIRE( pSegmentA != nullptr && pSegmentB != nullptr );
	SIRPoolAllocator poolA;
	SIRPoolAllocator poolB;
	REQUIRE( poolA.init( pSegmentA, 128 ) && poolB.init( pSegmentB, 128 ) );
	SIRMemorySegment *segmentsA[1] = { pSegmentA };
	SIRMemorySegment *segmentsB[1] = { pSegmentB };
	const sirReclaimOwnerConfig_t valid = { &poolA, segmentsA, 1, SIR_RECLAIM_POOL, 3, 0, 0, 0 };
	const sirReclaimOwnerConfig_t second = { &poolB, segmentsB, 1, SIR_RECLAIM_POOL, 1, 0, 0, 0 };
	uint32_t iA = UINT32_MAX;
	uint32_t iB = UINT32_MAX;
	REQUIRE( !pressure.registerOwner( valid, nullptr ) );
	sirReclaimOwnerConfig_t bad = valid;
	bad.pAllocator = nullptr;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	bad = valid;
	bad.ppSegments = nullptr;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	bad = valid;
	bad.nSegmentCount = 0;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	bad = valid;
	bad.nOwnerType = UINT32_MAX;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	bad = valid;
	bad.nOwnerType = SIR_RECLAIM_CACHE_EVICT;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	bad = valid;
	SIRMemorySegment *badSegments[1] = { nullptr };
	bad.ppSegments = badSegments;
	REQUIRE( !pressure.registerOwner( bad, &iA ) );
	REQUIRE( pressure.registerOwner( valid, &iA ) && iA == 0 );
	REQUIRE( !pressure.registerOwner( valid, &iB ) );
	REQUIRE( pressure.registerOwner( second, &iB ) && iB == 1 );
	REQUIRE( !pressure.registerOwner( second, &iB ) );
	REQUIRE( pressure.findNextPendingOwner() == UINT32_MAX );
	REQUIRE( pressure.serviceOwner( 2 ) == 0 );
	REQUIRE( !pressure.requestReclaim( 0 ) );
	REQUIRE( pressure.requestReclaim( nPage ) );
	REQUIRE( pressure.requestReclaim( nPage * 2 ) );
	sirPressureStats_t stats = pressure.getStats();
	REQUIRE( stats.nOwnerCount == 2 && stats.nPendingOwners == 2 );
	REQUIRE( stats.nUnfulfilledBytes == nPage * 4 );
	REQUIRE( stats.nRequestedBytes == nPage * 3 );
	REQUIRE( pressure.findNextPendingOwner() == iB );
	REQUIRE( pressure.findNextPendingOwner( 0 ) == UINT32_MAX );
	REQUIRE( pressure.findNextPendingOwner( 1 ) == iB );
	REQUIRE( pressure.cancelPending() );
	stats = pressure.getStats();
	REQUIRE( stats.nPendingOwners == 0 && stats.nCanceledBytes == nPage * 4 );
	REQUIRE( pressure.serviceOwner( iA ) == 0 );

	void *pLive = poolA.alloc();
	REQUIRE( pLive != nullptr );
	*( ( uint8_t * )pLive ) = 0x67;
	REQUIRE( pressure.requestReclaim( nPage ) );
	REQUIRE( pressure.serviceOwner( iA ) == 0 );
	REQUIRE( *( ( uint8_t * )pLive ) == 0x67 );
	REQUIRE( pressure.getStats().nPendingOwners >= 1 );
	REQUIRE( poolA.free( pLive ) );
	REQUIRE( pressure.serviceOwner( iA ) > 0 );
	REQUIRE( pressure.serviceOwner( iB ) == 0 );
	REQUIRE( pressure.getStats().nPendingOwners == 0 );
	REQUIRE( pressure.shutdown() );
	REQUIRE( !pressure.requestReclaim( nPage ) );
	REQUIRE( !pressure.shutdown() );
	REQUIRE( poolA.shutdown() && poolB.shutdown() );
	memory.shutdown();
	return 0;
}

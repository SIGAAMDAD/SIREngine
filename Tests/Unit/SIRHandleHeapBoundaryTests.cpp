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

#include <Core/Memory/Allocators/SIRHandleHeap.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sirHandleRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "handle heap coverage test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirHandleRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage > 0 );
	REQUIRE( SIRHandleHeap::getRequiredMetadataBytes( 0 ) == 0 );
	REQUIRE( SIRHandleHeap::getRequiredMetadataBytes( 128 ) > 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 2048, nPage * 1024, nPage * 64, 8 };
	REQUIRE( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "heap.extents", nPage * 512 );
	REQUIRE( pSegment != nullptr );
	SIRHandleHeap heap;
	REQUIRE( !heap.init( nullptr, 128 ) );
	REQUIRE( !heap.init( pSegment, 0 ) );
	REQUIRE( !heap.init( pSegment, UINT32_MAX ) );
	REQUIRE( heap.init( pSegment, 128 ) );
	REQUIRE( !heap.init( pSegment, 128 ) );
	sirMemoryHandle_t invalid = { UINT32_MAX, 0 };
	REQUIRE( heap.resolve( invalid ) == nullptr );
	REQUIRE( heap.pin( invalid ) == nullptr );
	REQUIRE( !heap.free( invalid ) );
	REQUIRE( !heap.unpin( invalid ) );
	REQUIRE( heap.alloc( 0, 16, &invalid ) == nullptr );
	REQUIRE( heap.alloc( 32, 3, &invalid ) == nullptr );
	REQUIRE( heap.alloc( 32, 16, nullptr ) == nullptr );
	REQUIRE( heap.alloc( UINT32_MAX, 16, &invalid ) == nullptr );

	sirMemoryHandle_t handles[128] = {};
	uint32_t sizes[128] = {};
	uint8_t pattern[128] = {};
	bool alive[128] = {};
	uint32_t nRandom = 0x27A538D1u;
	for ( uint32_t i = 0; i < 96; ++i ) {
		nRandom ^= nRandom << 13;
		nRandom ^= nRandom >> 17;
		nRandom ^= nRandom << 5;
		sizes[i] = 16 + nRandom % 496;
		pattern[i] = ( uint8_t )( 1 + nRandom % 251 );
		void *pMemory = heap.alloc( sizes[i], 16u << ( i % 4 ), &handles[i] );
		REQUIRE( pMemory != nullptr );
		memset( pMemory, pattern[i], sizes[i] );
		alive[i] = true;
	}
	REQUIRE( heap.getLiveCount() == 96 );
	REQUIRE( heap.resolve( handles[37] ) != nullptr );
	REQUIRE( heap.pin( handles[37] ) != nullptr );
	REQUIRE( !heap.free( handles[37] ) );
	REQUIRE( heap.compact() );
	REQUIRE( ( ( uint8_t * )heap.resolve( handles[37] ) )[0] == pattern[37] );
	REQUIRE( heap.unpin( handles[37] ) );
	REQUIRE( !heap.unpin( handles[37] ) );

	for ( uint32_t i = 0; i < 96; ++i ) {
		if ( i % 3 == 1 ) {
			REQUIRE( heap.free( handles[i] ) );
			alive[i] = false;
			REQUIRE( heap.resolve( handles[i] ) == nullptr );
			REQUIRE( !heap.free( handles[i] ) );
		}
	}
	const uint64_t nUpper = heap.getCompactionPayloadUpperBound();
	REQUIRE( nUpper > 0 );
	REQUIRE( !heap.compactWithinBudget( nUpper - 1 ) );
	REQUIRE( heap.compactWithinBudget( nUpper ) );
	for ( uint32_t i = 0; i < 96; ++i ) {
		if ( !alive[i] ) {
			continue;
		}
		void *pMemory = heap.resolve( handles[i] );
		REQUIRE( pMemory != nullptr );
		const uint8_t *pBytes = ( const uint8_t * )pMemory;
		for ( uint32_t j = 0; j < sizes[i]; ++j ) {
			REQUIRE( pBytes[j] == pattern[i] );
		}
	}
	for ( uint32_t i = 96; i < 128; ++i ) {
		sizes[i] = 48 + ( i % 5 ) * 32;
		pattern[i] = ( uint8_t )i;
		void *pMemory = heap.alloc( sizes[i], 32, &handles[i] );
		REQUIRE( pMemory != nullptr );
		memset( pMemory, pattern[i], sizes[i] );
		alive[i] = true;
	}
	for ( uint32_t i = 0; i < 128; ++i ) {
		const uint32_t index = ( i * 53 ) % 128;
		if ( !alive[index] ) {
			continue;
		}
		REQUIRE( heap.free( handles[index] ) );
		alive[index] = false;
	}
	REQUIRE( heap.getLiveCount() == 0 );
	REQUIRE( heap.trimUnusedPages() );
	REQUIRE( heap.shutdown() );
	memory.shutdown();
	return 0;
}

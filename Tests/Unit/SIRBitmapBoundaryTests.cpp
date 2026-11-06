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

#include <Core/Memory/Allocators/SIRBitmapAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sirBitmapRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "bitmap coverage test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirBitmapRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage > 0 );
	const uint64_t nReserve = nPage * 64;
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( nReserve, 64 ) != 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( 0, 64 ) == 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( nReserve, 0 ) == 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( nReserve, 64, 0 ) == 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( nReserve, 64, 3 ) == 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( nReserve, 64, 131072 ) == 0 );
	REQUIRE( SIRBitmapAllocator::getRequiredMetadataBytes( 1, 64 ) == 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 256, nPage * 128, nPage * 24, 8 };
	REQUIRE( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "bitmap.boundary", nReserve );
	REQUIRE( pSegment != nullptr );
	SIRBitmapAllocator bitmap;
	REQUIRE( bitmap.alloc() == nullptr );
	REQUIRE( !bitmap.free( nullptr ) );
	REQUIRE( !bitmap.trimUnusedPages() );
	REQUIRE( !bitmap.shutdown() );
	REQUIRE( !bitmap.init( nullptr, 64 ) );
	REQUIRE( !bitmap.init( pSegment, 0 ) );
	REQUIRE( !bitmap.init( pSegment, 64, 3 ) );
	REQUIRE( !bitmap.init( pSegment, 64, ( uint32_t )( nPage * 2 ) ) );
	REQUIRE( bitmap.init( pSegment, 64, 16, nPage ) );
	REQUIRE( !bitmap.init( pSegment, 64 ) );
	REQUIRE( bitmap.getSlotStride() == 64 );
	REQUIRE( bitmap.getCapacitySlots() > 256 );
	REQUIRE( !bitmap.precommitSlots( UINT32_MAX ) );
	REQUIRE( bitmap.precommitSlots( 200 ) );
	REQUIRE( bitmap.getActiveSlots() >= 200 );
	REQUIRE( bitmap.precommitSlots( 1 ) );

	void *pLive[192] = {};
	for ( uint32_t i = 0; i < 192; ++i ) {
		pLive[i] = bitmap.alloc();
		REQUIRE( pLive[i] != nullptr );
		memset( pLive[i], ( int )( i % 251 + 1 ), 64 );
	}
	REQUIRE( bitmap.getLiveSlots() == 192 );
	REQUIRE( !bitmap.shutdown() );
	REQUIRE( !bitmap.free( ( uint8_t * )pLive[0] + 1 ) );
	REQUIRE( !bitmap.free( ( void * )( ( uintptr_t )pSegment->getBase() - 1 ) ) );
	REQUIRE( !bitmap.free( nullptr ) );
	for ( uint32_t i = 0; i < 191; ++i ) {
		const uint32_t index = ( i * 53 ) % 191;
		REQUIRE( ( ( uint8_t * )pLive[index] )[0] == ( uint8_t )( index % 251 + 1 ) );
		REQUIRE( bitmap.free( pLive[index] ) );
		REQUIRE( !bitmap.free( pLive[index] ) );
	}
	REQUIRE( bitmap.getLiveSlots() == 1 );
	REQUIRE( bitmap.trimUnusedPages() );
	REQUIRE( bitmap.getActiveSlots() >= 192 );
	REQUIRE( ( ( uint8_t * )pLive[191] )[0] == 192 );
	REQUIRE( bitmap.free( pLive[191] ) );
	REQUIRE( bitmap.getLiveSlots() == 0 );
	REQUIRE( bitmap.trimUnusedPages() );
	REQUIRE( bitmap.getActiveSlots() == 0 );
	REQUIRE( bitmap.precommitSlots( 64 ) );
	void *pAgain = bitmap.alloc();
	REQUIRE( pAgain != nullptr );
	// The final allocation must be released before the allocator can shut down.
	REQUIRE( !bitmap.shutdown() );
	REQUIRE( bitmap.free( pAgain ) );
	REQUIRE( bitmap.shutdown() );
	memory.shutdown();
	return 0;
}

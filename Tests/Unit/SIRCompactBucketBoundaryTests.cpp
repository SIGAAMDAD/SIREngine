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

#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void sirBucketRequire( bool bCondition, uint32_t nLine ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "compact bucket coverage test failed at line %u\n", nLine );
		abort();
	}
}

#define REQUIRE( condition ) sirBucketRequire( ( condition ), __LINE__ )

int main( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	REQUIRE( nPage > 0 );
	const uint64_t nSegmentBytes = nPage * 128;
	sirCompactBucketConfig_t bucketConfig = { 2048, 16, 16384, 1, 0, 0 };
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bucketConfig ) != 0 );
	sirCompactBucketConfig_t bad = bucketConfig;
	bad.nSlabBytes = 16383;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nAlignment = 3;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nMaximumSmallBytes = 0;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nMaximumSmallBytes = 2049;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nWorkerSeats = 0;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nClassProfile = 4;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	bad = bucketConfig;
	bad.nSlabBytes = 1024;
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bad ) == 0 );
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( 0, bucketConfig ) == 0 );
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes + 1, bucketConfig ) == 0 );
	REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( UINT64_MAX, bucketConfig ) == 0 );

	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 2048, nPage * 1024, nPage * 128, 8 };
	REQUIRE( memory.init( config ) );
	const uint32_t sizes[] = { 1, 16, 24, 32, 64, 128, 255, 256, 257, 512, 1024, 2048 };
	for ( uint32_t profile = 0; profile < 4; ++profile ) {
		SIRMemorySegment *pSegment = memory.createSegment( "bucket.boundary", nSegmentBytes );
		REQUIRE( pSegment != nullptr );
		bucketConfig.nClassProfile = profile;
		bucketConfig.nAlignment = profile == 0 ? 8 : 16;
		REQUIRE( SIRCompactBucketAllocator::getRequiredMetadataBytes( nSegmentBytes, bucketConfig ) != 0 );
		SIRCompactBucketAllocator bucket;
		REQUIRE( !bucket.init( nullptr, bucketConfig ) );
		REQUIRE( bucket.init( pSegment, bucketConfig ) );
		REQUIRE( !bucket.init( pSegment, bucketConfig ) );
		REQUIRE( bucket.attach() );
		REQUIRE( bucket.getAttachedSeat() != UINT32_MAX );
		REQUIRE( bucket.getMaximumBytes() == 2048 );
		REQUIRE( bucket.getMinimumAlignment() == bucketConfig.nAlignment );
		REQUIRE( bucket.alloc( 0, 16 ) == nullptr );
		REQUIRE( bucket.alloc( 2049, 16 ) == nullptr );
		REQUIRE( bucket.alloc( 32, 3 ) == nullptr );
		REQUIRE( bucket.free( nullptr ) );
		for ( uint32_t size : sizes ) {
			void *pMemory = bucket.alloc( size, 16 );
			REQUIRE( pMemory != nullptr );
			REQUIRE( bucket.owns( pMemory ) );
			REQUIRE( bucket.getUsableSize( pMemory ) >= size );
			*( ( uint8_t * )pMemory ) = 0x58;
			REQUIRE( bucket.free( pMemory ) );
			REQUIRE( !bucket.free( pMemory ) );
		}
		REQUIRE( bucket.getLiveCount() == 0 );
		REQUIRE( bucket.drainRemote() == 0 );
		REQUIRE( bucket.servicePending( 32 ) == 0 );
		REQUIRE( bucket.trimEmptySlabs( 0 ) <= nSegmentBytes );
		REQUIRE( bucket.detach() );
		REQUIRE( bucket.shutdown() );
	}
	memory.shutdown();
	return 0;
}

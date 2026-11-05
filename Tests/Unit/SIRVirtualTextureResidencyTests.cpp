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

#include <Renderer/VirtualTexturing/SIRVirtualTextureResidency.h>
#include <stdio.h>

static bool check( bool bCondition, const char *pszCase ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
	}
	return bCondition;
}

static sirVirtualTexturePageKey_t makeKey( uint64_t nAsset, uint32_t nX = 0, uint64_t nRevision = 1 ) noexcept
{
	sirVirtualTexturePageKey_t key = {};
	key.source.m_nAssetId = nAsset;
	key.source.m_nTypeId = 1;
	key.nRevision = nRevision;
	key.nX = nX;
	return key;
}

static sirGraphicsCompletionSet_t makeCompletion( uint32_t nMask, uint64_t nDirect, uint64_t nCopy, uint32_t nGeneration = 11 ) noexcept
{
	sirGraphicsCompletionSet_t completion = {};
	completion.nDeviceGeneration = nGeneration;
	completion.nQueueMask = nMask;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = nDirect;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = nCopy;
	return completion;
}

static bool testLifecycle( void ) noexcept
{
	sirVirtualTextureSlot_t slots[2] = {};
	uint32_t buckets[4] = {};
	SIRVirtualTextureResidency cache;
	if ( !check( cache.initialize( { slots, buckets, 2, 4, 11 } ) == SIR_GRAPHICS_SUCCESS, "initialize" ) ) {
		return false;
	}

	const sirVirtualTexturePageKey_t first = makeKey( 100 );
	const sirVirtualTexturePageKey_t second = makeKey( 200 );
	const sirVirtualTexturePageKey_t third = makeKey( 300 );
	sirVirtualTextureHandle_t a = {};
	sirVirtualTextureHandle_t b = {};
	bool bCreated = false;
	if ( !check( cache.reserve( first, &a, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated, "initial reserve" ) ) {
		return false;
	}
	if ( !check( cache.reserve( first, &b, &bCreated ) == SIR_GRAPHICS_SUCCESS && !bCreated && b.nSlot == a.nSlot,
	             "duplicate feedback" ) ) {
		return false;
	}
	if ( !check( cache.find( first, &b ) == SIR_GRAPHICS_BUSY, "pending is not sampled" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t uploaded = makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 7 );
	if ( !check( cache.submitUpload( a, uploaded ) == SIR_GRAPHICS_SUCCESS, "upload admitted" ) ) {
		return false;
	}
	uint32_t nTransitions = 999;
	if ( !check( cache.collect( makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 6 ), &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 0,
	             "upload waits for copy fence" ) ) {
		return false;
	}
	if ( !check( cache.collect( uploaded, &nTransitions ) == SIR_GRAPHICS_SUCCESS && nTransitions == 1 && cache.getResidentCount() == 1,
	             "upload becomes resident" ) ) {
		return false;
	}
	if ( !check( cache.find( first, &b ) == SIR_GRAPHICS_SUCCESS && b.nSlot == a.nSlot, "resident visible" ) ) {
		return false;
	}
	if ( !check( cache.reserve( second, &b, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated, "second reserve" ) ) {
		return false;
	}
	sirVirtualTextureHandle_t scratch = {};
	if ( !check( cache.reserve( third, &scratch, &bCreated ) == SIR_GRAPHICS_CAPACITY_EXCEEDED, "bounded physical capacity" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t draw = makeCompletion( 1U << SIR_GRAPHICS_QUEUE_DIRECT, 9, 0 );
	if ( !check( cache.retire( a, draw ) == SIR_GRAPHICS_SUCCESS && cache.getRetiringCount() == 1, "retire after page-table unmap" ) ) {
		return false;
	}
	if ( !check( cache.find( first, &scratch ) == SIR_GRAPHICS_INVALID_HANDLE, "retired mapping removed" ) ) {
		return false;
	}
	if ( !check( cache.reserve( third, &scratch, &bCreated ) == SIR_GRAPHICS_CAPACITY_EXCEEDED, "retiring physical slot unavailable" ) ) {
		return false;
	}
	if ( !check( cache.collect( makeCompletion( ( 1U << SIR_GRAPHICS_QUEUE_COPY ) | ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ), 8, 7 ),
	                            &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 0,
	             "retire waits for draw fence" ) ) {
		return false;
	}
	if ( !check( cache.collect( makeCompletion( ( 1U << SIR_GRAPHICS_QUEUE_COPY ) | ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ), 9, 7 ),
	                            &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 1,
	             "retire reclaimed" ) ) {
		return false;
	}
	if ( !check( cache.reserve( third, &scratch, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated && scratch.nSlot == a.nSlot &&
	                 scratch.nGeneration != a.nGeneration,
	             "generation-safe reuse" ) ) {
		return false;
	}
	if ( !check( cache.cancel( a ) == SIR_GRAPHICS_INVALID_HANDLE && cache.cancel( scratch ) == SIR_GRAPHICS_SUCCESS,
	             "stale handle rejected" ) ) {
		return false;
	}
	if ( !check( cache.cancel( b ) == SIR_GRAPHICS_SUCCESS && cache.getActiveCount() == 0, "cancel unsubmitted request" ) ) {
		return false;
	}
	return true;
}

static bool testMultiQueueAndGeneration( void ) noexcept
{
	sirVirtualTextureSlot_t slots[1] = {};
	uint32_t buckets[2] = {};
	SIRVirtualTextureResidency cache;
	if ( !check( cache.initialize( { slots, buckets, 1, 2, 11 } ) == SIR_GRAPHICS_SUCCESS, "multi queue initialize" ) ) {
		return false;
	}
	sirVirtualTextureHandle_t handle = {};
	bool bCreated = false;
	if ( !check( cache.reserve( makeKey( 2 ), &handle, &bCreated ) == SIR_GRAPHICS_SUCCESS, "multi queue reserve" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t upload = makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 10 );
	if ( !check( cache.submitUpload( handle, upload ) == SIR_GRAPHICS_SUCCESS, "multi queue upload" ) ) {
		return false;
	}
	if ( !check( cache.retire( handle, makeCompletion( 1U << SIR_GRAPHICS_QUEUE_DIRECT, 15, 0 ) ) == SIR_GRAPHICS_SUCCESS,
	             "retire during in-flight upload" ) ) {
		return false;
	}
	uint32_t nTransitions = 0;
	if ( !check( cache.collect( makeCompletion( 1U << SIR_GRAPHICS_QUEUE_DIRECT, 15, 0 ), &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 0,
	             "missing copy completion" ) ) {
		return false;
	}
	if ( !check( cache.collect( makeCompletion( ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY ), 14, 10 ),
	                            &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 0,
	             "incomplete direct completion" ) ) {
		return false;
	}
	if ( !check( cache.collect( makeCompletion( ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY ), 15, 10 ),
	                            &nTransitions ) == SIR_GRAPHICS_SUCCESS &&
	                 nTransitions == 1,
	             "multi queue release" ) ) {
		return false;
	}
	if ( !check( cache.initialize( { slots, buckets, 1, 2, 11 } ) == SIR_GRAPHICS_BUSY, "same device generation cannot reset" ) ) {
		return false;
	}
	if ( !check( cache.initialize( { slots, buckets, 1, 2, 12 } ) == SIR_GRAPHICS_SUCCESS, "new device generation" ) ) {
		return false;
	}
	sirVirtualTextureHandle_t next = {};
	if ( !check( cache.reserve( makeKey( 2 ), &next, &bCreated ) == SIR_GRAPHICS_SUCCESS &&
	                 cache.cancel( handle ) == SIR_GRAPHICS_INVALID_HANDLE && next.nDeviceGeneration == 12,
	             "device generation stale handle" ) ) {
		return false;
	}
	return true;
}

static bool testInputsAndOldest( void ) noexcept
{
	sirVirtualTextureSlot_t slots[4] = {};
	uint32_t buckets[8] = {};
	SIRVirtualTextureResidency cache;
	if ( !check( cache.initialize( { slots, buckets, 4, 3, 11 } ) == SIR_GRAPHICS_INVALID_ARGUMENT, "reject non-power-of-two buckets" ) ) {
		return false;
	}
	if ( !check( cache.initialize( { slots, buckets, 4, 8, 11 } ) == SIR_GRAPHICS_SUCCESS, "valid storage" ) ) {
		return false;
	}
	sirVirtualTextureHandle_t handles[3] = {};
	bool bCreated = false;
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !check( cache.reserve( makeKey( 77, i ), &handles[i], &bCreated ) == SIR_GRAPHICS_SUCCESS, "allocate distinct tiles" ) ) {
			return false;
		}
		if ( !check( cache.submitUpload( handles[i], makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, i + 1 ) ) == SIR_GRAPHICS_SUCCESS,
		             "submit distinct tiles" ) ) {
			return false;
		}
	}
	uint32_t nTransitions = 0;
	cache.collect( makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 3 ), &nTransitions );
	if ( !check( nTransitions == 3, "all uploads complete" ) ) {
		return false;
	}
	sirVirtualTextureHandle_t oldest = {};
	if ( !check( cache.findOldest( &oldest ) == SIR_GRAPHICS_SUCCESS && oldest.nSlot == handles[0].nSlot, "oldest first" ) ) {
		return false;
	}
	if ( !check( cache.touch( handles[0] ) == SIR_GRAPHICS_SUCCESS && cache.findOldest( &oldest ) == SIR_GRAPHICS_SUCCESS &&
	                 oldest.nSlot == handles[1].nSlot,
	             "touch updates priority" ) ) {
		return false;
	}
	if ( !check( cache.submitUpload( handles[0], makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 9 ) ) == SIR_GRAPHICS_BUSY,
	             "resident cannot resubmit" ) ) {
		return false;
	}
	if ( !check( cache.retire( handles[2], makeCompletion( 1U << SIR_GRAPHICS_QUEUE_DIRECT, 0, 1 ) ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	             "reject zero-valued retirement" ) ) {
		return false;
	}
	if ( !check( cache.collect( makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 9, 7 ), &nTransitions ) == SIR_GRAPHICS_INVALID_ARGUMENT,
	             "reject wrong device completion" ) ) {
		return false;
	}
	return true;
}

static bool testBucketChurnAndInputFailures( void ) noexcept
{
	sirVirtualTextureSlot_t slots[2] = {};
	uint32_t buckets[4] = {};
	SIRVirtualTextureResidency cache;
	if ( !check( cache.initialize( { slots, buckets, 2, 4, 11 } ) == SIR_GRAPHICS_SUCCESS, "churn initialize" ) ) {
		return false;
	}
	for ( uint32_t nTile = 0; nTile < 2048; ++nTile ) {
		sirVirtualTextureHandle_t handle = {};
		bool bCreated = false;
		const sirVirtualTexturePageKey_t key = makeKey( 54, nTile );
		if ( !check( cache.reserve( key, &handle, &bCreated ) == SIR_GRAPHICS_SUCCESS && bCreated, "churn reserve" ) ) {
			return false;
		}
		sirVirtualTextureHandle_t repeat = {};
		if ( !check( cache.reserve( key, &repeat, &bCreated ) == SIR_GRAPHICS_SUCCESS && !bCreated && repeat.nSlot == handle.nSlot,
		             "churn dedup" ) ) {
			return false;
		}
		if ( !check( cache.cancel( handle ) == SIR_GRAPHICS_SUCCESS && cache.cancel( handle ) == SIR_GRAPHICS_INVALID_HANDLE,
		             "churn stale handle" ) ) {
			return false;
		}
		if ( !check( cache.find( key, &repeat ) == SIR_GRAPHICS_INVALID_HANDLE, "churn deleted lookup" ) ) {
			return false;
		}
	}
	if ( !check( cache.getActiveCount() == 0 && cache.getRetiringCount() == 0 && cache.getResidentCount() == 0, "churn counters" ) ) {
		return false;
	}

	sirVirtualTextureHandle_t handle = {};
	bool bCreated = false;
	if ( !check( cache.reserve( makeKey( 55 ), &handle, &bCreated ) == SIR_GRAPHICS_SUCCESS, "input reserve" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t empty = makeCompletion( 0, 0, 0 );
	if ( !check( cache.submitUpload( handle, empty ) == SIR_GRAPHICS_INVALID_ARGUMENT, "empty upload token" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t zero = makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COPY, 0, 0 );
	if ( !check( cache.submitUpload( handle, zero ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero-valued upload fence" ) ) {
		return false;
	}
	const sirGraphicsCompletionSet_t impossible = makeCompletion( 1U << SIR_GRAPHICS_QUEUE_COUNT, 0, 0 );
	if ( !check( cache.submitUpload( handle, impossible ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid upload queue" ) ) {
		return false;
	}

	// Exact slot-generation exhaustion permanently poisons the slot; wrapping to 1 would revive stale handles.
	slots[handle.nSlot].nGeneration = UINT32_MAX;
	handle.nGeneration = UINT32_MAX;
	if ( !check( cache.cancel( handle ) == SIR_GRAPHICS_SUCCESS && slots[handle.nSlot].nGeneration == 0,
	             "generation exhaustion poisons slot" ) ) {
		return false;
	}
	return check( cache.reserve( makeKey( 56 ), &handle, &bCreated ) == SIR_GRAPHICS_SUCCESS && handle.nSlot != 0,
	              "generation exhaustion chooses remaining slot" );
}

int main( void )
{
	if ( !testLifecycle() || !testMultiQueueAndGeneration() || !testInputsAndOldest() || !testBucketChurnAndInputFailures() ) {
		return 1;
	}
	printf( "SIR Virtual Texture residency tests passed\n" );
	return 0;
}

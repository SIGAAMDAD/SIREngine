#include <Core/Events/SIRChannelEventQueue.h>
#include <Core/Events/SIRDirtyEventSet.h>
#include <Core/Events/SIREventPhaseQueue.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIRStaticEvent.h>
#include <Core/Events/SIREventStream.h>
#include <Core/Events/SIREventScheduler.h>
#include <Core/Events/SIREventTimingWheel.h>
#include <atomic>
#include <new>
#include <stdlib.h>
#include <stdio.h>

static std::atomic<uint64_t> s_nAllocations{ 0 };
static volatile uint64_t s_nResult = 0;

void *operator new( size_t nBytes )
{
	s_nAllocations.fetch_add( 1, std::memory_order_relaxed );
	void *pResult = malloc( nBytes );
	if ( pResult == nullptr ) {
		throw std::bad_alloc();
	}
	return pResult;
}

void *operator new[]( size_t nBytes )
{
	s_nAllocations.fetch_add( 1, std::memory_order_relaxed );
	void *pResult = malloc( nBytes );
	if ( pResult == nullptr ) {
		throw std::bad_alloc();
	}
	return pResult;
}

void operator delete( void *pData ) noexcept
{
	free( pData );
}
void operator delete[]( void *pData ) noexcept
{
	free( pData );
}
void operator delete( void *pData, size_t ) noexcept
{
	free( pData );
}
void operator delete[]( void *pData, size_t ) noexcept
{
	free( pData );
}

static void sirEventNoAllocSum( void *pContext, const uint32_t& nValue ) noexcept
{
	*( ( uint64_t * )pContext ) += nValue;
}

static void sirEventNoAllocDirty( void *pContext, sirCompactEventRef_t ref ) noexcept
{
	*( ( uint64_t * )pContext ) += ref.nSlot;
}

static void sirEventNoAllocPhase( void *pContext, sirEventTargetRef_t, const uint32_t& nValue ) noexcept
{
	*( ( uint64_t * )pContext ) += nValue;
}

int main( void )
{
	SIRStaticEvent<uint32_t, 2> staticEvent;
	SIRSPSCEventRing<uint32_t, 64> ring;
	SIREventStream<uint32_t, 64> stream;
	SIRChannelEventQueue<uint32_t, 64> queue;
	SIRDirtyEventSet<512, false> dirty;
	SIREventPhaseQueue<uint32_t, 2, 64> phases;
	SIREventTimingWheel<uint32_t, 16, 64> wheel;
	uint64_t nSum = 0;
	if ( !staticEvent.subscribe( &nSum, sirEventNoAllocSum ) || !staticEvent.seal() || !queue.bind( &nSum, sirEventNoAllocSum ) ) {
		return 2;
	}
	const uint64_t nBefore = s_nAllocations.load( std::memory_order_relaxed );
	for ( uint32_t n = 0; n < 100000; ++n ) {
		const uint32_t nValue = ( n % 16 ) + 1;
		const uint32_t szBatch[] = { nValue, nValue + 1, nValue + 2 };
		if ( ring.enqueueBatch( szBatch, 3 ) != 3 )
			return 3;
		const uint32_t nDrained = ring.drainSpans( [&]( const uint32_t *pValues, uint32_t nCount ) noexcept {
			for ( uint32_t i = 0; i < nCount; ++i )
				nSum += pValues[i];
		} );
		if ( nDrained != 3 )
			return 4;
		if ( !stream.append( nValue ) || stream.drain( &nSum, sirEventNoAllocSum ) != 1 )
			return 5;
		if ( !queue.enqueue( nValue ) || queue.pump() != 1 )
			return 6;
		if ( !dirty.mark( nValue ) || dirty.drain( &nSum, sirEventNoAllocDirty ) != 1 )
			return 7;
		if ( !phases.enqueue( 0, nValue ) || phases.pumpPhase( 0, &nSum, sirEventNoAllocPhase, nullptr ) != 1 )
			return 8;
		sirEventScheduleHandle_t handle{};
		if ( !wheel.schedule( &nSum, sirEventNoAllocSum, nValue, n, 1, 0, handle ) || wheel.pump( n + 1 ) != 1 )
			return 9;
		staticEvent.publish( nValue );
	}
	// General-range timer heap now uses a bounded, inline SIRStaticVector.
	// Exercise full admission, indexed cancellation, ordering, and reuse.
	{
		SIREventScheduler<uint32_t, 4> scheduler;
		sirEventScheduleHandle_t szHandles[4] = {};
		const uint64_t nPreviousSum = nSum;
		for ( uint32_t iTimer = 0; iTimer < 4; ++iTimer ) {
			if ( !scheduler.schedule( &nSum, sirEventNoAllocSum, iTimer + 1, 100, iTimer + 1, 0, szHandles[iTimer] ) ) {
				return 10;
			}
		}
		sirEventScheduleHandle_t rejected = {};
		if ( scheduler.pendingCount() != 4 || scheduler.schedule( &nSum, sirEventNoAllocSum, 5, 100, 5, 0, rejected ) ) {
			return 11;
		}
		if ( !scheduler.cancel( szHandles[2] ) || scheduler.pendingCount() != 3 ) {
			return 12;
		}
		if ( scheduler.pump( 101 ) != 1 || scheduler.pump( 102 ) != 1 || scheduler.pump( 104 ) != 1 || scheduler.pendingCount() != 0 ||
		     nSum != nPreviousSum + 7 ) {
			return 13;
		}
		if ( !scheduler.schedule( &nSum, sirEventNoAllocSum, 9, 200, 1, 0, rejected ) || scheduler.cancel( szHandles[2] ) ||
		     scheduler.pump( 201 ) != 1 || scheduler.pendingCount() != 0 ) {
			return 14;
		}
	}
	s_nResult = nSum;
	const uint64_t nAfter = s_nAllocations.load( std::memory_order_relaxed );
	if ( nAfter != nBefore || s_nResult == 0 ) {
		fprintf( stderr, "Hot-path allocations: %llu\n", ( unsigned long long )( nAfter - nBefore ) );
		return 1;
	}
	printf( "SIREvent hot-path allocation smoke: PASS (100000 iterations, 0 new/new[] calls)\n" );
	return 0;
}

#include <Core/Events/SIRChannelEventQueue.h>
#include <Core/Events/SIRDirtyEventSet.h>
#include <Core/Events/SIREventPhaseQueue.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIRStaticEvent.h>
#include <Core/Events/SIREventStream.h>
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
	s_nResult = nSum;
	const uint64_t nAfter = s_nAllocations.load( std::memory_order_relaxed );
	if ( nAfter != nBefore || s_nResult == 0 ) {
		fprintf( stderr, "Hot-path allocations: %llu\n", ( unsigned long long )( nAfter - nBefore ) );
		return 1;
	}
	printf( "SIREvent hot-path allocation smoke: PASS (100000 iterations, 0 new/new[] calls)\n" );
	return 0;
}

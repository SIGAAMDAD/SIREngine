#include <Core/Events/SIRAsyncEventChannel.h>
#include <Core/Events/SIRDeferredEventChannel.h>
#include <Core/Events/SIREventMailbox.h>
#include <Core/Events/SIRMPSCEventQueue.h>
#include <Core/Events/SIREventPhaseQueue.h>
#include <Core/Events/SIREventQueue.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIRStaticEvent.h>
#include <Core/Events/SIREventTimingWheel.h>
#include <Core/Events/SIREventScheduler.h>
#include <gtest/gtest.h>
#include <atomic>
#include <thread>

TEST( SIREventOptimizationGoogleTest, SpscBatchWrapAndCapacity )
{
	SIRSPSCEventRing<uint32_t, 8> ring;
	const uint32_t first[] = { 0, 1, 2, 3, 4, 5 };
	const uint32_t second[] = { 6, 7, 8, 9, 10, 11, 12 };
	uint32_t out[12]{};
	EXPECT_EQ( ring.enqueueBatch( first, 6 ), 6U );
	EXPECT_EQ( ring.dequeueBatch( out, 4 ), 4U );
	EXPECT_EQ( ring.enqueueBatch( second, 7 ), 6U );
	EXPECT_EQ( ring.enqueueBatch( second, 1 ), 0U );
	EXPECT_EQ( ring.dequeueBatch( out + 4, 8 ), 8U );
	for ( uint32_t i = 0; i < 12; ++i ) {
		EXPECT_EQ( out[i], i );
	}
	EXPECT_EQ( ring.approximateCount(), 0U );
	ring.stop();
	EXPECT_EQ( ring.enqueueBatch( first, 6 ), 0U );
	EXPECT_EQ( ring.tryEnqueue( 12 ), SIR_EVENT_ENQUEUE_STOPPED );
}

TEST( SIREventOptimizationGoogleTest, SpscBatchProtectsActivePayloadFromReentrantDrain )
{
	SIRSPSCEventRing<uint32_t, 8> ring;
	const uint32_t values[] = { 1, 2, 3, 4 };
	ASSERT_EQ( ring.enqueueBatch( values, 4 ), 4U );
	uint32_t nTotal = 0;
	uint32_t nNested = 100;
	const uint32_t nCount = ring.drainBatch( [&]( const uint32_t &value ) noexcept {
		nTotal += value;
		nNested = ring.drainBatch( []( const uint32_t & ) noexcept {} );
		uint32_t nOutput = 0;
		EXPECT_FALSE( ring.tryDequeue( nOutput ) );
	} );
	EXPECT_EQ( nCount, 4U );
	EXPECT_EQ( nTotal, 10U );
	EXPECT_EQ( nNested, 0U );
}

TEST( SIREventOptimizationGoogleTest, StaticEventSealsAndDoesNotMutate )
{
	SIRStaticEvent<uint32_t, 2> event;
	uint32_t nSum = 0;
	auto callback = []( void *pContext, const uint32_t &value ) noexcept { *( ( uint32_t * )pContext ) += value; };
	EXPECT_FALSE( event.publish( 1 ) );
	ASSERT_TRUE( event.subscribe( &nSum, callback ) );
	ASSERT_TRUE( event.subscribe( &nSum, callback ) );
	EXPECT_FALSE( event.subscribe( &nSum, callback ) );
	ASSERT_TRUE( event.seal() );
	EXPECT_FALSE( event.seal() );
	EXPECT_FALSE( event.subscribe( &nSum, callback ) );
	EXPECT_TRUE( event.publish( 3 ) );
	EXPECT_EQ( nSum, 6U );
	EXPECT_EQ( event.subscriberCount(), 2U );
}

TEST( SIREventOptimizationGoogleTest, DeferredAndAsyncBatchPreserveDeliveryBoundaries )
{
	SIRDeferredEventChannel<uint32_t, 8, 1> deferred;
	SIRAsyncEventChannel<uint32_t, 8, 1> async;
	uint32_t nA = 0;
	uint32_t nB = 0;
	auto callback = []( void *pContext, const uint32_t &value ) noexcept { *( ( uint32_t * )pContext ) += value; };
	ASSERT_TRUE( deferred.subscribe( &nA, callback ).isValid() );
	ASSERT_TRUE( async.subscribe( &nB, callback ).isValid() );
	const uint32_t values[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	EXPECT_EQ( deferred.enqueueBatch( values, 9 ), 8U );
	EXPECT_EQ( async.publishBatch( values, 9 ), 8U );
	EXPECT_EQ( nA, 0U );
	EXPECT_EQ( nB, 0U );
	EXPECT_EQ( deferred.pump(), 8U );
	EXPECT_EQ( async.pump(), 8U );
	EXPECT_EQ( nA, 36U );
	EXPECT_EQ( nB, 36U );
}

TEST( SIREventOptimizationGoogleTest, MailboxValidatesProducerAndRoundRobinFairness )
{
	SIREventMailbox<uint32_t, 2, 8> mailbox;
	const sirEventProducerHandle_t a = mailbox.registerProducer();
	const sirEventProducerHandle_t b = mailbox.registerProducer();
	ASSERT_TRUE( a.isValid() );
	ASSERT_TRUE( b.isValid() );
	EXPECT_FALSE( mailbox.registerProducer().isValid() );
	EXPECT_EQ( mailbox.tryPublish( a, 1 ), SIR_EVENT_ENQUEUE_STOPPED );
	ASSERT_TRUE( mailbox.seal() );
	EXPECT_FALSE( mailbox.seal() );
	const uint32_t payloads[] = { 10, 11, 12, 13 };
	ASSERT_EQ( mailbox.publishBatch( a, payloads, 4 ), 4U );
	ASSERT_EQ( mailbox.publishBatch( b, payloads, 4 ), 4U );
	uint32_t nSum = 0;
	EXPECT_EQ( mailbox.pump( [&]( const uint32_t &value ) noexcept { nSum += value; }, 3, 1 ), 2U );
	EXPECT_EQ( mailbox.pump( [&]( const uint32_t &value ) noexcept { nSum += value; }, 6, 3 ), 6U );
	EXPECT_EQ( nSum, 92U );
	EXPECT_EQ( mailbox.approximatePendingCount(), 0U );
	const sirEventProducerHandle_t bogus{ &mailbox, a.nSlot, a.nGeneration + 1 };
	EXPECT_EQ( mailbox.tryPublish( bogus, 4 ), SIR_EVENT_ENQUEUE_STOPPED );
	mailbox.stop();
	EXPECT_EQ( mailbox.tryPublish( b, 4 ), SIR_EVENT_ENQUEUE_STOPPED );
}

typedef struct sirEventStampedTest_s
{
	uint32_t nProducer;
	uint32_t nSequence;
} sirEventStampedTest_t;

TEST( SIREventOptimizationGoogleTest, FourProducersMaintainIndependentFifoUnderPressure )
{
	constexpr uint32_t NUM_PRODUCERS = 4;
	constexpr uint32_t NUM_PER_PRODUCER = 30000;
	SIREventMailbox<sirEventStampedTest_t, NUM_PRODUCERS, 128> mailbox;
	sirEventProducerHandle_t tokens[NUM_PRODUCERS]{};
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		tokens[i] = mailbox.registerProducer();
		ASSERT_TRUE( tokens[i].isValid() );
	}
	ASSERT_TRUE( mailbox.seal() );
	std::thread producers[NUM_PRODUCERS];
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		producers[i] = std::thread( [&, i] {
			sirEventStampedTest_t payloads[16]{};
			uint32_t nNext = 0;
			while ( nNext < NUM_PER_PRODUCER ) {
				const uint32_t nCount = NUM_PER_PRODUCER - nNext < 16 ? NUM_PER_PRODUCER - nNext : 16;
				for ( uint32_t j = 0; j < nCount; ++j ) {
					payloads[j] = { i, nNext + j };
				}
				const uint32_t nAccepted = mailbox.publishBatch( tokens[i], payloads, nCount );
				nNext += nAccepted;
				if ( nAccepted == 0 ) {
					std::this_thread::yield();
				}
			}
		} );
	}
	uint32_t nSeen[NUM_PRODUCERS]{};
	uint32_t nDelivered = 0;
	bool bCorrect = true;
	while ( nDelivered < NUM_PRODUCERS * NUM_PER_PRODUCER ) {
		const uint32_t nBatch = mailbox.pump(
		    [&]( const sirEventStampedTest_t &value ) noexcept {
			    if ( value.nProducer >= NUM_PRODUCERS || value.nSequence != nSeen[value.nProducer] ) {
				    bCorrect = false;
			    } else {
				    ++nSeen[value.nProducer];
			    }
		    },
		    512, 32 );
		nDelivered += nBatch;
		if ( nBatch == 0 ) {
			std::this_thread::yield();
		}
	}
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		producers[i].join();
	}
	mailbox.stop();
	EXPECT_TRUE( bCorrect );
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		EXPECT_EQ( nSeen[i], NUM_PER_PRODUCER );
	}
}

TEST( SIREventOptimizationGoogleTest, PhasedDeliveryRejectsStaleAndDefersReentrantEvents )
{
	SIREventPhaseQueue<uint32_t, 3, 8> queue;
	typedef struct state_s
	{
		SIREventPhaseQueue<uint32_t, 3, 8> *pQueue;
		uint32_t nGeneration;
		uint32_t nSum;
	} state_t;
	state_t state{ &queue, 2, 0 };
	const sirEventTargetRef_t fresh{ 42, 2, 1 };
	const sirEventTargetRef_t stale{ 42, 1, 1 };
	ASSERT_TRUE( queue.enqueue( 1, 7, fresh ) );
	ASSERT_TRUE( queue.enqueue( 1, 11, stale ) );
	ASSERT_TRUE( queue.enqueue( 1, 13 ) );
	EXPECT_FALSE( queue.enqueue( 4, 17 ) );
	const sirEventTargetRef_t malformed{ 42, 0, 1 };
	EXPECT_FALSE( queue.enqueue( 1, 19, malformed ) );
	const sirEventTargetRef_t invalidBroadcast{ 0, 0, 1 };
	EXPECT_FALSE( queue.enqueue( 1, 19, invalidBroadcast ) );
	auto validator = []( void *pContext, sirEventTargetRef_t ref ) noexcept {
		return ref.nIdentifier == 42 && ref.nGeneration == ( ( state_t * )pContext )->nGeneration && ref.nDomain == 1;
	};
	auto callback = []( void *pContext, sirEventTargetRef_t, const uint32_t &value ) noexcept {
		state_t &s = *( ( state_t * )pContext );
		s.nSum += value;
		if ( value == 7 ) {
			s.pQueue->enqueue( 1, 5 );
		}
	};
	EXPECT_EQ( queue.pumpPhase( 1, &state, callback, validator ), 2U );
	EXPECT_EQ( state.nSum, 20U );
	EXPECT_EQ( queue.staleDiscardCount(), 1U );
	EXPECT_EQ( queue.pendingCount( 1 ), 1U );
	EXPECT_EQ( queue.pumpPhase( 1, &state, callback, validator ), 1U );
	EXPECT_EQ( state.nSum, 25U );
	state.nGeneration = 3;
	ASSERT_TRUE( queue.enqueue( 1, 100, fresh ) );
	EXPECT_EQ( queue.pumpPhase( 1, &state, callback, validator ), 0U );
	EXPECT_EQ( queue.staleDiscardCount(), 2U );
}

TEST( SIREventOptimizationGoogleTest, SharedMpscIngressBackpressureAndFifo )
{
	SIRMPSCEventQueue<uint32_t, 8> ingress;
	for ( uint32_t i = 0; i < 8; ++i ) {
		ASSERT_EQ( ingress.tryEnqueue( i ), SIR_EVENT_ENQUEUE_OK );
	}
	EXPECT_EQ( ingress.tryEnqueue( 9 ), SIR_EVENT_ENQUEUE_FULL );
	uint32_t nTotal = 0;
	uint32_t nNext = 0;
	EXPECT_EQ( ingress.drain( [&]( const uint32_t &value ) noexcept {
		EXPECT_EQ( value, nNext++ );
		nTotal += value;
	} ),
	           8U );
	EXPECT_EQ( nTotal, 28U );
	uint32_t nValue = 0;
	EXPECT_FALSE( ingress.tryDequeue( nValue ) );
	ingress.stop();
	EXPECT_EQ( ingress.tryEnqueue( 1 ), SIR_EVENT_ENQUEUE_STOPPED );
}

TEST( SIREventOptimizationGoogleTest, SharedMpscIngressStressFourProducers )
{
	constexpr uint32_t NUM_PRODUCERS = 4;
	constexpr uint32_t NUM_PER_PRODUCER = 15000;
	SIRMPSCEventQueue<sirEventStampedTest_t, 128> ingress;
	std::thread producers[NUM_PRODUCERS];
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		producers[i] = std::thread( [&, i] {
			for ( uint32_t n = 0; n < NUM_PER_PRODUCER; ++n ) {
				while ( ingress.tryEnqueue( { i, n } ) == SIR_EVENT_ENQUEUE_FULL ) {
					std::this_thread::yield();
				}
			}
		} );
	}
	uint32_t nNext[NUM_PRODUCERS]{};
	bool bOrdered = true;
	uint32_t nDelivered = 0;
	while ( nDelivered < NUM_PRODUCERS * NUM_PER_PRODUCER ) {
		nDelivered += ingress.drain(
		    [&]( const sirEventStampedTest_t &event ) noexcept {
			    if ( event.nProducer >= NUM_PRODUCERS || nNext[event.nProducer] != event.nSequence ) {
				    bOrdered = false;
			    } else {
				    ++nNext[event.nProducer];
			    }
		    },
		    256 );
	}
	for ( uint32_t i = 0; i < NUM_PRODUCERS; ++i ) {
		producers[i].join();
		EXPECT_EQ( nNext[i], NUM_PER_PRODUCER );
	}
	EXPECT_TRUE( bOrdered );
	ingress.stop();
}

TEST( SIREventOptimizationGoogleTest, TimingWheelCancellationBudgetAndRecurrence )
{
	SIREventTimingWheel<uint32_t, 8, 16> wheel;
	uint32_t nSum = 0;
	auto callback = []( void *pContext, const uint32_t &value ) noexcept { *( ( uint32_t * )pContext ) += value; };
	sirEventScheduleHandle_t cancelled{};
	sirEventScheduleHandle_t periodic{};
	sirEventScheduleHandle_t oneShot{};
	ASSERT_TRUE( wheel.schedule( &nSum, callback, 11, 100, 3, 0, cancelled ) );
	ASSERT_TRUE( wheel.schedule( &nSum, callback, 2, 100, 2, 3, periodic ) );
	ASSERT_TRUE( wheel.schedule( &nSum, callback, 7, 100, 2, 0, oneShot ) );
	ASSERT_TRUE( wheel.cancel( cancelled ) );
	EXPECT_FALSE( wheel.cancel( cancelled ) );
	EXPECT_EQ( wheel.pump( 101 ), 0U );
	EXPECT_EQ( wheel.pump( 102, 1 ), 1U );
	EXPECT_EQ( wheel.pump( 102, 1 ), 1U );
	EXPECT_EQ( nSum, 9U );
	EXPECT_EQ( wheel.pendingCount(), 1U );
	EXPECT_EQ( wheel.pump( 104 ), 0U );
	EXPECT_EQ( wheel.pump( 105 ), 1U );
	EXPECT_EQ( nSum, 11U );
	EXPECT_EQ( wheel.pump( 400 ), 1U );
	EXPECT_EQ( nSum, 13U );
	EXPECT_TRUE( wheel.cancel( periodic ) );
	EXPECT_EQ( wheel.pump( 405 ), 0U );
	EXPECT_EQ( wheel.pendingCount(), 0U );
}

TEST( SIREventOptimizationGoogleTest, TimingWheelClearFromCallbackInvalidatesCollectedTimers )
{
	SIREventTimingWheel<uint32_t, 8, 16> wheel;
	uint32_t nDelivered = 0;
	struct context_s
	{
		SIREventTimingWheel<uint32_t, 8, 16> *pWheel;
		uint32_t *pCount;
	} context{ &wheel, &nDelivered };
	auto callback = []( void *pContext, const uint32_t & ) noexcept {
		context_s &s = *( ( context_s * )pContext );
		++*s.pCount;
		s.pWheel->clear();
	};
	sirEventScheduleHandle_t a{};
	sirEventScheduleHandle_t b{};
	ASSERT_TRUE( wheel.schedule( &context, callback, 1, 10, 1, 0, a ) );
	ASSERT_TRUE( wheel.schedule( &context, callback, 2, 10, 1, 0, b ) );
	EXPECT_EQ( wheel.pump( 11 ), 1U );
	EXPECT_EQ( nDelivered, 1U );
	EXPECT_EQ( wheel.pendingCount(), 0U );
	EXPECT_FALSE( wheel.cancel( a ) );
	EXPECT_FALSE( wheel.cancel( b ) );
	sirEventScheduleHandle_t c{};
	ASSERT_TRUE( wheel.schedule( &context, callback, 3, 11, 1, 0, c ) );
	EXPECT_TRUE( c.isValid() );
}

TEST( SIREventOptimizationGoogleTest, TimingWheelBoundsAndExhaustion )
{
	SIREventTimingWheel<uint32_t, 2, 8> wheel;
	uint32_t nSum = 0;
	auto callback = []( void *pContext, const uint32_t &value ) noexcept { *( ( uint32_t * )pContext ) += value; };
	sirEventScheduleHandle_t h{};
	EXPECT_FALSE( wheel.schedule( &nSum, callback, 1, 0, 8, 0, h ) );
	ASSERT_TRUE( wheel.schedule( &nSum, callback, 1, UINT64_MAX - 3, 1, 1, h ) );
	EXPECT_EQ( wheel.pump( UINT64_MAX ), 1U );
	EXPECT_EQ( wheel.pendingCount(), 0U );
	EXPECT_EQ( nSum, 1U );
	EXPECT_FALSE( wheel.cancel( h ) );
}

TEST( SIREventOptimizationGoogleTest, TimingWheelMatchesHeapOneShotRandomizedTimeline )
{
	constexpr uint32_t MAX_EVENTS = 8192;
	SIREventScheduler<uint32_t, 64> heap;
	SIREventTimingWheel<uint32_t, 64, 32> wheel;
	struct context_s
	{
		uint32_t nCalls[MAX_EVENTS]{};
	} heapState, wheelState;
	struct pair_s
	{
		sirEventScheduleHandle_t heapHandle;
		sirEventScheduleHandle_t wheelHandle;
	};
	pair_s handles[MAX_EVENTS]{};
	uint32_t nHandles = 0;
	uint32_t nSeed = 0x12345abc;
	auto next = [&nSeed]() noexcept -> uint32_t {
		nSeed = nSeed * 1664525U + 1013904223U;
		return nSeed;
	};
	auto callback = []( void *pContext, const uint32_t &nId ) noexcept { ++( ( context_s * )pContext )->nCalls[nId]; };
	for ( uint64_t nTick = 0; nTick < 2500; ++nTick ) {
		const uint32_t nInsert = next() % 4;
		for ( uint32_t i = 0; i < nInsert; ++i ) {
			const uint32_t nId = nHandles + 1;
			const uint64_t nDelay = 1 + ( next() % 15 );
			sirEventScheduleHandle_t a{}, b{};
			const bool bHeap = heap.schedule( &heapState, callback, nId, nTick, nDelay, 0, a );
			const bool bWheel = wheel.schedule( &wheelState, callback, nId, nTick, nDelay, 0, b );
			EXPECT_EQ( bHeap, bWheel );
			if ( bHeap ) {
				handles[nHandles++] = { a, b };
			}
		}
		if ( nHandles != 0 && next() % 3 == 0 ) {
			const uint32_t nIndex = next() % nHandles;
			EXPECT_EQ( heap.cancel( handles[nIndex].heapHandle ), wheel.cancel( handles[nIndex].wheelHandle ) );
		}
		EXPECT_EQ( heap.pump( nTick + 1 ), wheel.pump( nTick + 1 ) );
	}
	EXPECT_EQ( heap.pump( 10000 ), wheel.pump( 10000 ) );
	for ( uint32_t i = 0; i < nHandles; ++i ) {
		EXPECT_EQ( heapState.nCalls[i + 1], wheelState.nCalls[i + 1] );
		EXPECT_TRUE( heapState.nCalls[i + 1] <= 1 );
	}
}

TEST( SIREventOptimizationGoogleTest, PhaseQueueReportsGlobalCapacityAndDefersSafely )
{
	SIREventPhaseQueue<uint32_t, 2, 2> queue;
	ASSERT_TRUE( queue.enqueue( 0, 1 ) );
	ASSERT_TRUE( queue.enqueue( 0, 2 ) );
	EXPECT_FALSE( queue.enqueue( 1, 4 ) );
	struct context_s
	{
		SIREventPhaseQueue<uint32_t, 2, 2> *pQueue;
		uint32_t nSum;
		uint32_t nFailed;
		uint32_t nAccepted;
	} context{ &queue, 0, 0, 0 };
	auto callback = []( void *pContext, sirEventTargetRef_t, const uint32_t &value ) noexcept {
		context_s &s = *( ( context_s * )pContext );
		s.nSum += value;
		if ( s.pQueue->enqueue( 0, value + 10 ) ) {
			++s.nAccepted;
		} else {
			++s.nFailed;
		}
	};
	EXPECT_EQ( queue.pumpPhase( 0, &context, callback, nullptr ), 2U );
	EXPECT_EQ( context.nFailed, 1U );
	EXPECT_EQ( context.nAccepted, 1U );
	EXPECT_EQ( context.nSum, 3U );
	EXPECT_EQ( queue.pendingCount( 0 ), 1U );
	queue.clearPending( 0 );
	EXPECT_EQ( queue.pendingCount( 0 ), 0U );
	const sirEventTargetRef_t target{ 18, 1, 0 };
	ASSERT_TRUE( queue.enqueue( 0, 4, target ) );
	EXPECT_EQ( queue.pumpPhase( 0, &context, callback, nullptr ), 0U );
	EXPECT_EQ( queue.staleDiscardCount(), 1U );
}

TEST( SIREventOptimizationGoogleTest, SealedStaticEventSupportsConcurrentReaders )
{
	SIRStaticEvent<uint32_t, 2> event;
	std::atomic<uint64_t> nTotal{ 0 };
	auto callback = []( void *pContext, const uint32_t &value ) noexcept {
		( ( std::atomic<uint64_t> * )pContext )->fetch_add( value, std::memory_order_relaxed );
	};
	ASSERT_TRUE( event.subscribe( &nTotal, callback ) );
	ASSERT_TRUE( event.seal() );
	std::thread readers[4];
	for ( uint32_t i = 0; i < 4; ++i ) {
		readers[i] = std::thread( [&event] {
			for ( uint32_t j = 0; j < 20000; ++j ) {
				event.publish( 1 );
			}
		} );
	}
	for ( std::thread &thread : readers ) {
		thread.join();
	}
	EXPECT_EQ( nTotal.load( std::memory_order_relaxed ), 80000U );
}

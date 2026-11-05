#include <Core/Events/SIRAsyncEventChannel.h>
#include <Core/Events/SIRChannelEventQueue.h>
#include <Core/Events/SIRDeferredEventChannel.h>
#include <Core/Events/SIRDirtyEventSet.h>
#include <Core/Events/SIRExternalEventQueue.h>
#include <Core/Events/SIREventMailbox.h>
#include <Core/Events/SIREventPhaseQueue.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIREventScheduler.h>
#include <Core/Events/SIREventTimingWheel.h>
#include <gtest/gtest.h>
#include <assert.h>
#include <deque>
#include <atomic>
#include <stdint.h>
#include <thread>

static void sirFurtherSum( void *pContext, const uint32_t& value ) noexcept
{
	*( ( uint64_t * )pContext ) += value;
}

typedef struct sirFurtherDirtyContext_s
{
	SIRDirtyEventSet<130> *pSet;
	uint32_t nCount;
	uint32_t nLastSlot;
	uint32_t nGeneration;
	bool bClear;
} sirFurtherDirtyContext_t;

static bool sirFurtherDirtyValidate( void *pContext, sirCompactEventRef_t ref ) noexcept
{
	return ref.nGeneration == ( ( sirFurtherDirtyContext_t * )pContext )->nGeneration;
}

static void sirFurtherDirtyCallback( void *pContext, sirCompactEventRef_t ref ) noexcept
{
	sirFurtherDirtyContext_t *pState = ( sirFurtherDirtyContext_t * )pContext;
	++pState->nCount;
	pState->nLastSlot = ref.nSlot;
	if ( pState->bClear ) {
		pState->bClear = false;
		pState->pSet->clear();
		pState->pSet->mark( 129, 2 );
	} else if ( ref.nSlot == 1 ) {
		pState->pSet->mark( 1, 2 );
	}
}

TEST( SIREventFurtherOptimizationGoogleTest, CompactResourceReferencesHaveEightByteRepresentation )
{
	static_assert( sizeof( sirCompactEventRef_t ) == 8, "Unexpected compact resource reference size" );
	static_assert( sizeof( SIRDirtyEventSet<4096, false> ) < 600, "Bit-only dirty set should fit under 600 bytes" );
	const sirCompactEventRef_t valid{ 11, 3 };
	const sirCompactEventRef_t invalid{ 0, 3 };
	EXPECT_TRUE( valid.isValid() );
	EXPECT_FALSE( invalid.isValid() );
}

TEST( SIREventFurtherOptimizationGoogleTest, DirtyCoalescingTracksLastGenerationAndRejectsStale )
{
	SIRDirtyEventSet<130> set;
	sirFurtherDirtyContext_t state{ &set, 0, 0, 2, false };
	ASSERT_TRUE( set.mark( 64, 1 ) );
	ASSERT_TRUE( set.mark( 64, 2 ) );
	ASSERT_TRUE( set.mark( 65, 1 ) );
	ASSERT_TRUE( set.mark( 130, 2 ) );
	EXPECT_FALSE( set.mark( 0 ) );
	EXPECT_FALSE( set.mark( 131 ) );
	EXPECT_FALSE( set.mark( 65, 0 ) );
	EXPECT_EQ( set.coalescedCount(), 1U );
	EXPECT_EQ( set.pendingCount(), 3U );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback, sirFurtherDirtyValidate, 2 ), 1U );
	EXPECT_EQ( set.pendingCount(), 1U );
	EXPECT_EQ( set.staleCount(), 1U );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback, sirFurtherDirtyValidate ), 1U );
	EXPECT_EQ( state.nCount, 2U );
	EXPECT_EQ( set.pendingCount(), 0U );
}

TEST( SIREventFurtherOptimizationGoogleTest, DirtyReentrantMarksAreDeferredToNextDrain )
{
	SIRDirtyEventSet<130> set;
	sirFurtherDirtyContext_t state{ &set, 0, 0, 2, false };
	ASSERT_TRUE( set.mark( 1, 2 ) );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback ), 1U );
	EXPECT_EQ( set.pendingCount(), 1U );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback ), 1U );
	EXPECT_EQ( state.nCount, 2U );
	EXPECT_EQ( set.pendingCount(), 1U );
	set.clear();
	EXPECT_EQ( set.pendingCount(), 0U );
}

TEST( SIREventFurtherOptimizationGoogleTest, DirtyClearDuringCallbackDiscardsSnapshotWithoutDroppingNewWork )
{
	SIRDirtyEventSet<130> set;
	sirFurtherDirtyContext_t state{ &set, 0, 0, 2, true };
	ASSERT_TRUE( set.mark( 1, 2 ) );
	ASSERT_TRUE( set.mark( 64, 2 ) );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback ), 1U );
	EXPECT_EQ( state.nCount, 1U );
	EXPECT_EQ( set.pendingCount(), 1U );
	EXPECT_EQ( set.drain( &state, sirFurtherDirtyCallback ), 1U );
	EXPECT_EQ( state.nLastSlot, 129U );
}

TEST( SIREventFurtherOptimizationGoogleTest, BitOnlyDirtySetCoalescesEntireCapacity )
{
	SIRDirtyEventSet<4096, false> set;
	for ( uint32_t i = 1; i <= 4096; ++i ) {
		ASSERT_TRUE( set.mark( i ) );
		ASSERT_TRUE( set.mark( i ) );
	}
	EXPECT_EQ( set.pendingCount(), 4096U );
	EXPECT_EQ( set.coalescedCount(), 4096U );
	uint32_t nLast = 0;
	auto cb = []( void *pContext, sirCompactEventRef_t ref ) noexcept {
		uint32_t *pValue = ( uint32_t * )pContext;
		assert( ref.nSlot > *pValue );
		*pValue = ref.nSlot;
	};
	EXPECT_EQ( set.drain( &nLast, cb ), 4096U );
	EXPECT_EQ( nLast, 4096U );
	EXPECT_EQ( set.pendingCount(), 0U );
}

typedef struct sirFurtherQueueContext_s
{
	SIRChannelEventQueue<uint32_t, 8> *pQueue;
	uint32_t szValues[16];
	uint32_t nCount;
	bool bClearOnce;
} sirFurtherQueueContext_t;

static void sirFurtherQueueCallback( void *pContext, const uint32_t& value ) noexcept
{
	sirFurtherQueueContext_t& ctx = *( ( sirFurtherQueueContext_t * )pContext );
	ctx.szValues[ctx.nCount++] = value;
	if ( ctx.bClearOnce ) {
		ctx.bClearOnce = false;
		ctx.pQueue->clear();
		ctx.pQueue->enqueue( 99 );
	}
}

TEST( SIREventFurtherOptimizationGoogleTest, ChannelQueueStoresOneHandlerAndWrapsWithoutExtraMetadata )
{
	static_assert( sizeof( SIRChannelEventQueue<uint32_t, 256> ) < 1100, "Per-slot channel storage too large" );
	SIRChannelEventQueue<uint32_t, 8> queue;
	sirFurtherQueueContext_t ctx{ &queue, {}, 0, false };
	EXPECT_TRUE( queue.bind( &ctx, sirFurtherQueueCallback ) );
	const uint32_t first[] = { 1, 2, 3, 4, 5, 6, 7 };
	ASSERT_EQ( queue.enqueueBatch( first, 7 ), 7U );
	EXPECT_EQ( queue.pump( 5 ), 5U );
	const uint32_t second[] = { 8, 9, 10, 11, 12, 13, 14 };
	EXPECT_EQ( queue.enqueueBatch( second, 7 ), 6U );
	EXPECT_EQ( queue.pendingCount(), 8U );
	EXPECT_EQ( queue.pump(), 8U );
	for ( uint32_t i = 0; i < 13; ++i ) {
		EXPECT_EQ( ctx.szValues[i], i + 1 );
	}
	EXPECT_EQ( queue.pendingCount(), 0U );
}

TEST( SIREventFurtherOptimizationGoogleTest, ChannelClearInsideCallbackPreservesRequeuedRecord )
{
	SIRChannelEventQueue<uint32_t, 8> queue;
	sirFurtherQueueContext_t ctx{ &queue, {}, 0, true };
	ASSERT_TRUE( queue.bind( &ctx, sirFurtherQueueCallback ) );
	ASSERT_TRUE( queue.enqueue( 1 ) );
	ASSERT_TRUE( queue.enqueue( 2 ) );
	EXPECT_EQ( queue.pump(), 1U );
	EXPECT_EQ( queue.pendingCount(), 1U );
	EXPECT_EQ( queue.pump(), 1U );
	EXPECT_EQ( ctx.nCount, 2U );
	EXPECT_EQ( ctx.szValues[0], 1U );
	EXPECT_EQ( ctx.szValues[1], 99U );
}

TEST( SIREventFurtherOptimizationGoogleTest, SpscZeroCopyLeaseSupportsPartialCommitAndWraparound )
{
	SIRSPSCEventRing<uint32_t, 8> ring;
	const uint32_t szFirst[] = { 1, 2, 3, 4, 5, 6 };
	ASSERT_EQ( ring.enqueueBatch( szFirst, 6 ), 6U );
	sirEventReadSpan_s<uint32_t> first{}, second{};
	ASSERT_EQ( ring.acquireReadSpans( first, second ), 6U );
	EXPECT_EQ( first.nCount, 6U );
	EXPECT_EQ( second.nCount, 0U );
	EXPECT_FALSE( ring.commitRead( 7 ) );
	EXPECT_EQ( first.pData[0], 1U );
	EXPECT_TRUE( ring.commitRead( 5 ) );
	const uint32_t szSecond[] = { 7, 8, 9, 10, 11, 12, 13 };
	ASSERT_EQ( ring.enqueueBatch( szSecond, 7 ), 7U );
	ASSERT_EQ( ring.acquireReadSpans( first, second ), 8U );
	EXPECT_EQ( first.nCount, 3U );
	EXPECT_EQ( second.nCount, 5U );
	EXPECT_EQ( first.pData[0], 6U );
	EXPECT_EQ( second.pData[4], 13U );
	uint32_t nRejected = 0;
	EXPECT_FALSE( ring.tryDequeue( nRejected ) );
	EXPECT_TRUE( ring.commitRead( 8 ) );
	EXPECT_FALSE( ring.commitRead( 0 ) );
	EXPECT_EQ( ring.approximateCount(), 0U );
}

TEST( SIREventFurtherOptimizationGoogleTest, SpscSpanDrainConsumesTwoRangesWithoutCopy )
{
	SIRSPSCEventRing<uint32_t, 8> ring;
	const uint32_t a[] = { 1, 2, 3, 4, 5, 6 };
	const uint32_t b[] = { 7, 8, 9, 10, 11, 12, 13 };
	ASSERT_EQ( ring.enqueueBatch( a, 6 ), 6U );
	uint32_t skip[5]{};
	ASSERT_EQ( ring.dequeueBatch( skip, 5 ), 5U );
	ASSERT_EQ( ring.enqueueBatch( b, 7 ), 7U );
	uint32_t nCalls = 0;
	uint32_t nNext = 6;
	EXPECT_EQ( ring.drainSpans( [&]( const uint32_t *pData, uint32_t nCount ) noexcept {
		++nCalls;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			EXPECT_EQ( pData[i], nNext++ );
		}
	} ),
	           8U );
	EXPECT_EQ( nCalls, 2U );
	EXPECT_EQ( nNext, 14U );
}

TEST( SIREventFurtherOptimizationGoogleTest, MailboxBatchSpansCarryProducerIdentity )
{
	SIREventMailbox<uint32_t, 2, 8> mailbox;
	const sirEventProducerHandle_t a = mailbox.registerProducer();
	const sirEventProducerHandle_t b = mailbox.registerProducer();
	ASSERT_TRUE( mailbox.seal() );
	const uint32_t x[] = { 1, 2, 3 };
	const uint32_t y[] = { 10, 20 };
	ASSERT_EQ( mailbox.publishBatch( a, x, 3 ), 3U );
	ASSERT_EQ( mailbox.publishBatch( b, y, 2 ), 2U );
	uint32_t nFirst = 0, nSecond = 0;
	EXPECT_EQ( mailbox.pumpSpans( [&]( uint32_t nProducer, const uint32_t *pValues, uint32_t nCount ) noexcept {
		for ( uint32_t i = 0; i < nCount; ++i ) {
			if ( nProducer == 0 ) {
				nFirst += pValues[i];
			} else {
				nSecond += pValues[i];
			}
		}
	} ),
	           5U );
	EXPECT_EQ( nFirst, 6U );
	EXPECT_EQ( nSecond, 30U );
}

TEST( SIREventFurtherOptimizationGoogleTest, OptInInterestFilteringSkipsUnobservedAsyncAndDeferredEvents )
{
	SIRAsyncEventChannel<uint32_t, 8, 2, true> async;
	SIRDeferredEventChannel<uint32_t, 8, 2, true> deferred;
	SIRAsyncEventChannel<uint32_t, 8, 2> lossless;
	uint64_t nAsync = 0, nDeferred = 0;
	EXPECT_EQ( async.tryPublish( 3 ), SIR_EVENT_ENQUEUE_NO_LISTENERS );
	EXPECT_FALSE( deferred.enqueue( 3 ) );
	EXPECT_EQ( lossless.tryPublish( 3 ), SIR_EVENT_ENQUEUE_OK );
	EXPECT_EQ( async.approximatePendingCount(), 0U );
	const sirSubscriptionHandle_t a = async.subscribe( &nAsync, sirFurtherSum );
	const sirSubscriptionHandle_t b = deferred.subscribe( &nDeferred, sirFurtherSum );
	ASSERT_TRUE( a.isValid() );
	ASSERT_TRUE( b.isValid() );
	EXPECT_EQ( async.tryPublish( 5 ), SIR_EVENT_ENQUEUE_OK );
	EXPECT_TRUE( deferred.enqueue( 7 ) );
	EXPECT_EQ( async.pump(), 1U );
	EXPECT_EQ( deferred.pump(), 1U );
	EXPECT_EQ( nAsync, 5U );
	EXPECT_EQ( nDeferred, 7U );
	ASSERT_TRUE( async.unsubscribe( a ) );
	ASSERT_TRUE( deferred.unsubscribe( b ) );
	EXPECT_EQ( async.tryPublish( 9 ), SIR_EVENT_ENQUEUE_NO_LISTENERS );
	EXPECT_FALSE( deferred.enqueue( 9 ) );
}

TEST( SIREventFurtherOptimizationGoogleTest, BudgetedPhaseProcessingPreservesOldThenReentrantOrder )
{
	SIREventPhaseQueue<uint32_t, 2, 8> queue;
	struct state_s
	{
		SIREventPhaseQueue<uint32_t, 2, 8> *pQueue;
		uint32_t szValues[8];
		uint32_t nCount;
	} state{ &queue, {}, 0 };
	auto cb = []( void *pContext, sirEventTargetRef_t, const uint32_t& value ) noexcept {
		state_s& ctx = *( ( state_s * )pContext );
		ctx.szValues[ctx.nCount++] = value;
		if ( value == 1 ) {
			ctx.pQueue->enqueue( 0, 9 );
		}
	};
	ASSERT_TRUE( queue.enqueue( 0, 1 ) );
	ASSERT_TRUE( queue.enqueue( 0, 2 ) );
	ASSERT_TRUE( queue.enqueue( 0, 3 ) );
	EXPECT_EQ( queue.pumpPhase( 0, &state, cb, nullptr, 1 ), 1U );
	EXPECT_EQ( queue.pendingCount( 0 ), 3U );
	EXPECT_EQ( queue.pumpPhase( 0, &state, cb, nullptr, 2 ), 2U );
	EXPECT_EQ( queue.pendingCount( 0 ), 1U );
	EXPECT_EQ( queue.pumpPhase( 0, &state, cb, nullptr ), 1U );
	EXPECT_EQ( state.nCount, 4U );
	EXPECT_EQ( state.szValues[0], 1U );
	EXPECT_EQ( state.szValues[1], 2U );
	EXPECT_EQ( state.szValues[2], 3U );
	EXPECT_EQ( state.szValues[3], 9U );
}

TEST( SIREventFurtherOptimizationGoogleTest, TimingWheelBitmapSkipsSparseBucketsAndWraps )
{
	SIREventTimingWheel<uint32_t, 8, 16> wheel;
	uint64_t nTotal = 0;
	sirEventScheduleHandle_t a{}, b{};
	ASSERT_TRUE( wheel.schedule( &nTotal, sirFurtherSum, 4, 100, 12, 0, a ) );
	ASSERT_TRUE( wheel.schedule( &nTotal, sirFurtherSum, 7, 100, 2, 0, b ) );
	EXPECT_EQ( wheel.pump( 101 ), 0U );
	EXPECT_EQ( wheel.pump( 102 ), 1U );
	EXPECT_EQ( wheel.pump( 111 ), 0U );
	EXPECT_EQ( wheel.pump( 112 ), 1U );
	EXPECT_EQ( nTotal, 11U );
	EXPECT_EQ( wheel.pendingCount(), 0U );
	ASSERT_TRUE( wheel.schedule( &nTotal, sirFurtherSum, 6, 112, 15, 0, a ) );
	EXPECT_EQ( wheel.pump( 5000 ), 1U );
	EXPECT_EQ( nTotal, 17U );
}

TEST( SIREventFurtherOptimizationGoogleTest, SpscSpanStressMaintainsStrictFifoAcrossThreads )
{
	constexpr uint32_t N = 100000;
	SIRSPSCEventRing<uint32_t, 256> ring;
	std::atomic<bool> bBroken{ false };
	std::thread producer( [&] {
		uint32_t nNext = 1;
		uint32_t szBatch[32];
		while ( nNext <= N ) {
			const uint32_t nRemaining = N - nNext + 1;
			const uint32_t nWant = nRemaining < 32 ? nRemaining : 32;
			for ( uint32_t i = 0; i < nWant; ++i ) {
				szBatch[i] = nNext + i;
			}
			const uint32_t nAccepted = ring.enqueueBatch( szBatch, nWant );
			nNext += nAccepted;
			if ( nAccepted == 0 ) {
				std::this_thread::yield();
			}
		}
	} );
	uint32_t nExpected = 1;
	while ( nExpected <= N ) {
		const uint32_t nRead = ring.drainSpans(
		    [&]( const uint32_t *pItems, uint32_t nCount ) noexcept {
			    for ( uint32_t i = 0; i < nCount; ++i ) {
				    if ( pItems[i] != nExpected++ ) {
					    bBroken.store( true, std::memory_order_relaxed );
				    }
			    }
		    },
		    128 );
		if ( nRead == 0 ) {
			std::this_thread::yield();
		}
	}
	producer.join();
	EXPECT_FALSE( bBroken.load( std::memory_order_relaxed ) );
	EXPECT_EQ( nExpected, N + 1 );
}

TEST( SIREventFurtherOptimizationGoogleTest, ColdSizedExternalQueueAvoidsInlineStorageAndSupportsReconfiguration )
{
	static_assert( sizeof( SIRExternalEventQueue<uint32_t> ) <= 64, "External queue object should be compact" );
	uint32_t szStorage[13]{};
	uint64_t nSum = 0;
	SIRExternalEventQueue<uint32_t> queue;
	EXPECT_EQ( queue.capacity(), 0U );
	EXPECT_FALSE( queue.enqueue( 1 ) );
	ASSERT_TRUE( queue.init( szStorage, 13, &nSum, sirFurtherSum ) );
	EXPECT_FALSE( queue.init( szStorage, 13, &nSum, sirFurtherSum ) );
	EXPECT_EQ( queue.capacity(), 13U );
	const uint32_t szFirst[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	ASSERT_EQ( queue.enqueueBatch( szFirst, 9 ), 9U );
	EXPECT_EQ( queue.pump( 7 ), 7U );
	const uint32_t szSecond[] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };
	ASSERT_EQ( queue.enqueueBatch( szSecond, 12 ), 11U );
	EXPECT_EQ( queue.pump(), 13U );
	EXPECT_EQ( nSum, 210U );
	EXPECT_EQ( queue.pendingCount(), 0U );
	EXPECT_TRUE( queue.shutdown() );
	uint32_t szSmaller[3]{};
	ASSERT_TRUE( queue.init( szSmaller, 3, &nSum, sirFurtherSum ) );
	EXPECT_TRUE( queue.enqueue( 22 ) );
	EXPECT_EQ( queue.pump(), 1U );
	EXPECT_EQ( nSum, 232U );
}

TEST( SIREventFurtherOptimizationGoogleTest, RandomizedDirtySetMatchesReferenceAcrossEpochs )
{
	SIRDirtyEventSet<130> set;
	struct context_s
	{
		uint32_t nObserved[130];
		uint32_t nLast;
		bool bSorted;
	} state{};
	auto cb = []( void *pContext, sirCompactEventRef_t ref ) noexcept {
		context_s& ctx = *( ( context_s * )pContext );
		if ( ref.nSlot <= ctx.nLast || ref.nSlot > 130 ) {
			ctx.bSorted = false;
		}
		ctx.nLast = ref.nSlot;
		ctx.nObserved[ref.nSlot - 1] = ref.nGeneration;
	};
	uint32_t nSeed = 0x419736faU;
	auto next = [&nSeed]() noexcept -> uint32_t { return nSeed = nSeed * 1664525U + 1013904223U; };
	for ( uint32_t nEpoch = 1; nEpoch <= 350; ++nEpoch ) {
		uint32_t nExpected[130]{};
		uint32_t nUnique = 0;
		for ( uint32_t nChange = 0; nChange < 200; ++nChange ) {
			const uint32_t nSlot = 1 + ( next() % 130 );
			const uint32_t nGeneration = 1 + ( next() % 1000 );
			if ( nExpected[nSlot - 1] == 0 ) {
				++nUnique;
			}
			nExpected[nSlot - 1] = nGeneration;
			ASSERT_TRUE( set.mark( nSlot, nGeneration ) );
		}
		state = {};
		state.bSorted = true;
		EXPECT_EQ( set.pendingCount(), nUnique );
		EXPECT_EQ( set.drain( &state, cb ), nUnique );
		EXPECT_TRUE( state.bSorted );
		for ( uint32_t n = 0; n < 130; ++n ) {
			EXPECT_EQ( state.nObserved[n], nExpected[n] );
		}
		EXPECT_EQ( set.pendingCount(), 0U );
	}
}

TEST( SIREventFurtherOptimizationGoogleTest, RandomizedChannelFifoMatchesReference )
{
	SIRChannelEventQueue<uint32_t, 31> queue;
	struct context_s
	{
		std::deque<uint32_t> *pReference;
		bool bCorrect;
	} context{};
	std::deque<uint32_t> reference;
	context.pReference = &reference;
	context.bCorrect = true;
	auto callback = []( void *pContext, const uint32_t& value ) noexcept {
		context_s& ctx = *( ( context_s * )pContext );
		if ( ctx.pReference->empty() || ctx.pReference->front() != value ) {
			ctx.bCorrect = false;
		} else {
			ctx.pReference->pop_front();
		}
	};
	ASSERT_TRUE( queue.bind( &context, callback ) );
	uint32_t nSeed = 0x1234587U;
	for ( uint32_t n = 1; n <= 20000; ++n ) {
		nSeed = nSeed * 1664525U + 1013904223U;
		if ( ( nSeed & 3 ) != 0 ) {
			const bool bExpected = reference.size() < 31;
			EXPECT_EQ( queue.enqueue( n ), bExpected );
			if ( bExpected ) {
				reference.push_back( n );
			}
		} else {
			const uint32_t nBudget = 1 + ( nSeed % 11 );
			const uint32_t nExpected = reference.size() < nBudget ? ( uint32_t )reference.size() : nBudget;
			EXPECT_EQ( queue.pump( nBudget ), nExpected );
		}
		EXPECT_EQ( queue.pendingCount(), ( uint32_t )reference.size() );
	}
	const uint32_t nRemaining = ( uint32_t )reference.size();
	EXPECT_EQ( queue.pump(), nRemaining );
	EXPECT_TRUE( reference.empty() );
	EXPECT_TRUE( context.bCorrect );
}

TEST( SIREventFurtherOptimizationGoogleTest, CompactPhaseStorageValidatesEightByteHandles )
{
	static_assert( sizeof( SIRCompactPhaseQueue<uint32_t, 4, 256> ) < sizeof( SIREventPhaseQueue<uint32_t, 4, 256> ),
	               "Compact phase records should use less memory" );
	SIRCompactPhaseQueue<uint32_t, 2, 8> queue;
	struct context_s
	{
		uint32_t nExpectedGeneration;
		uint32_t nTotal;
	} state{ 5, 0 };
	const sirCompactEventRef_t fresh{ 42, 5 };
	const sirCompactEventRef_t stale{ 42, 4 };
	EXPECT_FALSE( queue.enqueue( 0, 9, { 0, 5 } ) );
	ASSERT_TRUE( queue.enqueue( 0, 3, fresh ) );
	ASSERT_TRUE( queue.enqueue( 0, 7, stale ) );
	ASSERT_TRUE( queue.enqueue( 0, 11 ) );
	auto validator = []( void *pContext, sirCompactEventRef_t ref ) noexcept {
		return ref.nGeneration == ( ( context_s * )pContext )->nExpectedGeneration;
	};
	auto callback = []( void *pContext, sirCompactEventRef_t, const uint32_t& payload ) noexcept {
		( ( context_s * )pContext )->nTotal += payload;
	};
	EXPECT_EQ( queue.pumpPhase( 0, &state, callback, validator ), 2U );
	EXPECT_EQ( queue.staleDiscardCount(), 1U );
	EXPECT_EQ( state.nTotal, 14U );
}

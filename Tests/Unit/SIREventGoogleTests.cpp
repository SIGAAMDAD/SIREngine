#include <Core/Events/SIREvent.h>
#include <Core/Events/SIRAsyncEventChannel.h>
#include <Core/Events/SIRDeferredEventChannel.h>
#include <Core/Events/SIREventFilter.h>
#include <Core/Events/SIREventQueue.h>
#include <Core/Events/SIREventRegistry.h>
#include <Core/Events/SIREventScheduler.h>
#include <Core/Events/SIREventStream.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Events/SIRSubscriptionGroup.h>
#include <Core/SIRApplication.h>
#include <gtest/gtest.h>
#include <thread>

struct sirEventTestContext_s
{
	SIREvent<int, 8> *pEvent = nullptr;
	sirSubscriptionHandle_t removed{};
	int nTotal = 0;
	int nCalls = 0;
	bool bNested = false;
};

static void SIRTestCountEvent( void *pContext, const int &value ) noexcept
{
	sirEventTestContext_s &context = *( ( sirEventTestContext_s * )pContext );
	context.nTotal += value;
	++context.nCalls;
}

static void SIRTestRemoveNested( void *pContext, const int &value ) noexcept
{
	sirEventTestContext_s &context = *( ( sirEventTestContext_s * )pContext );
	context.nTotal += value;
	++context.nCalls;
	if ( !context.bNested ) {
		context.bNested = true;
		context.pEvent->unsubscribe( context.removed );
		context.pEvent->publish( 10 );
	}
}

TEST( SIREventGoogleTest, SuppressRemovedSubscriberInNestedDispatch )
{
	SIREvent<int, 8> event;
	sirEventTestContext_s context;
	context.pEvent = &event;
	const sirSubscriptionHandle_t first = event.subscribe( &context, SIRTestRemoveNested );
	context.removed = event.subscribe( &context, SIRTestCountEvent );
	ASSERT_TRUE( first.isValid() );
	ASSERT_TRUE( context.removed.isValid() );
	event.publish( 2 );
	EXPECT_EQ( context.nTotal, 12 );
	EXPECT_EQ( context.nCalls, 2 );
	EXPECT_EQ( event.subscriberCount(), 1U );
	EXPECT_FALSE( event.unsubscribe( context.removed ) );
}

TEST( SIREventGoogleTest, DeferSubscriptionsUntilOutermostPublication )
{
	struct context_t
	{
		SIREvent<int, 4> *pEvent;
		uint32_t nAdded;
		uint32_t nInvoked;
	} context{ nullptr, 0, 0 };
	SIREvent<int, 4> event;
	context.pEvent = &event;
	auto subscriber = []( void *pContext, const int & ) noexcept {
		context_t &state = *( ( context_t * )pContext );
		++state.nInvoked;
		if ( state.nAdded++ == 0 ) {
			state.pEvent->subscribe( &state, []( void *p, const int & ) noexcept { ++( ( context_t * )p )->nInvoked; } );
			state.pEvent->publish( 3 );
		}
	};
	ASSERT_TRUE( event.subscribe( &context, subscriber ).isValid() );
	event.publish( 2 );
	EXPECT_EQ( context.nInvoked, 2U );
	EXPECT_EQ( event.subscriberCount(), 2U );
	event.publish( 1 );
	EXPECT_EQ( context.nInvoked, 4U );
}

TEST( SIREventGoogleTest, GroupInvalidatesSubscriptionsWithoutAllocation )
{
	SIREvent<int, 8> event;
	sirEventTestContext_s context;
	SIRSubscriptionGroup<2> group;
	const sirSubscriptionHandle_t handle = event.subscribe( &context, SIRTestCountEvent );
	ASSERT_TRUE( group.add( event, handle ) );
	event.publish( 7 );
	group.clear();
	event.publish( 7 );
	EXPECT_EQ( context.nTotal, 7 );
	EXPECT_EQ( event.subscriberCount(), 0U );
}

TEST( SIREventGoogleTest, PreservePriorityAndReserveInflightSlot )
{
	struct context_t
	{
		SIREventQueue<int, 3> *pQueue;
		int nOrder;
	} context{};
	SIREventQueue<int, 3> queue;
	context.pQueue = &queue;
	auto callback = []( void *pContext, const int &value ) noexcept {
		context_t &state = *( ( context_t * )pContext );
		state.nOrder = state.nOrder * 10 + value;
		if ( value == 2 ) {
			state.pQueue->clear();
			state.pQueue->enqueue(
			    &state, []( void *p, const int &n ) noexcept { ( ( context_t * )p )->nOrder = ( ( context_t * )p )->nOrder * 10 + n; }, 3 );
		}
	};
	ASSERT_TRUE( queue.enqueue( &context, callback, 1, SIR_EVENT_PRIORITY_LOW ) );
	ASSERT_TRUE( queue.enqueue( &context, callback, 2, SIR_EVENT_PRIORITY_CRITICAL ) );
	EXPECT_EQ( queue.processAll(), 2U );
	EXPECT_EQ( context.nOrder, 23 );
}

TEST( SIREventGoogleTest, StreamBuffersReentrantAppends )
{
	SIREventStream<int, 4> stream;
	ASSERT_TRUE( stream.append( 1 ) );
	uint32_t nDelivered = 0;
	struct context_t
	{
		SIREventStream<int, 4> *pStream;
		uint32_t *pDelivered;
	} context{ &stream, &nDelivered };
	auto callback = []( void *pContext, const int &payload ) noexcept {
		context_t &state = *( ( context_t * )pContext );
		*state.pDelivered += payload;
		if ( payload == 1 ) {
			state.pStream->append( 2 );
		}
	};
	EXPECT_EQ( stream.drain( &context, callback ), 1U );
	EXPECT_EQ( nDelivered, 1U );
	EXPECT_EQ( stream.drain( &context, callback ), 1U );
	EXPECT_EQ( nDelivered, 3U );
}

TEST( SIREventGoogleTest, SpscRingPreservesFullCapacityAndFifoUnderThreads )
{
	SIRSPSCEventRing<uint32_t, 128> ring;
	for ( uint32_t i = 0; i < 128; ++i ) {
		ASSERT_EQ( ring.tryEnqueue( i ), SIR_EVENT_ENQUEUE_OK );
	}
	EXPECT_EQ( ring.tryEnqueue( 129 ), SIR_EVENT_ENQUEUE_FULL );
	for ( uint32_t i = 0; i < 128; ++i ) {
		uint32_t nReceived = UINT32_MAX;
		ASSERT_TRUE( ring.tryDequeue( nReceived ) );
		ASSERT_EQ( nReceived, i );
	}
	std::thread producer( [&ring] {
		for ( uint32_t i = 1; i <= 100000; ++i ) {
			while ( ring.tryEnqueue( i ) == SIR_EVENT_ENQUEUE_FULL ) {
				std::this_thread::yield();
			}
		}
	} );
	for ( uint32_t i = 1; i <= 100000; ++i ) {
		uint32_t nReceived = 0;
		while ( !ring.tryDequeue( nReceived ) ) {
			std::this_thread::yield();
		}
		ASSERT_EQ( nReceived, i );
	}
	producer.join();
	ring.stop();
	EXPECT_EQ( ring.tryEnqueue( 1 ), SIR_EVENT_ENQUEUE_STOPPED );
}

TEST( SIREventGoogleTest, SchedulerCancellationAndRecurringDueTime )
{
	SIREventScheduler<int, 4> scheduler;
	int nSum = 0;
	sirEventScheduleHandle_t scheduled{};
	auto callback = []( void *pContext, const int &value ) noexcept { *( ( int * )pContext ) += value; };
	ASSERT_TRUE( scheduler.schedule( &nSum, callback, 3, 0, 5, 10, scheduled ) );
	EXPECT_EQ( scheduler.pump( 4 ), 0U );
	EXPECT_EQ( scheduler.pump( 5 ), 1U );
	EXPECT_EQ( scheduler.pump( 30 ), 1U );
	EXPECT_EQ( nSum, 6 );
	EXPECT_TRUE( scheduler.cancel( scheduled ) );
	EXPECT_FALSE( scheduler.cancel( scheduled ) );
	EXPECT_EQ( scheduler.pendingCount(), 0U );
}

TEST( SIREventGoogleTest, RegistryHandlesAreGenerationChecked )
{
	SIRApplication application;
	ASSERT_TRUE( application.initialize() );
	sirEventHandle_t handle{};
	ASSERT_TRUE( application.getEventRegistry().registerEvent( 17, "resource.ready", 8, 8, handle ) );
	sirRegisteredEvent_t descriptor{};
	ASSERT_TRUE( application.getEventRegistry().getDescriptor( handle, descriptor ) );
	EXPECT_EQ( descriptor.nTypeId, 17U );
	application.shutdown();
	EXPECT_FALSE( application.getEventRegistry().getDescriptor( handle, descriptor ) );
	EXPECT_EQ( application.getEventRegistry().count(), 0U );
}

TEST( SIREventGoogleTest, AsyncAndDeferredChannelsHaveExplicitDeliveryBoundaries )
{
	SIRAsyncEventChannel<int, 8, 4> async;
	SIRDeferredEventChannel<int, 8, 4> deferred;
	sirEventTestContext_s first;
	sirEventTestContext_s second;
	ASSERT_TRUE( async.subscribe( &first, SIRTestCountEvent ).isValid() );
	ASSERT_TRUE( deferred.subscribe( &second, SIRTestCountEvent ).isValid() );
	ASSERT_EQ( async.tryPublish( 5 ), SIR_EVENT_ENQUEUE_OK );
	ASSERT_TRUE( deferred.enqueue( 7 ) );
	ASSERT_EQ( first.nTotal, 0 );
	ASSERT_EQ( second.nTotal, 0 );
	EXPECT_EQ( async.pump(), 1U );
	EXPECT_EQ( deferred.pump(), 1U );
	EXPECT_EQ( first.nTotal, 5 );
	EXPECT_EQ( second.nTotal, 7 );
	async.stop();
	EXPECT_EQ( async.tryPublish( 2 ), SIR_EVENT_ENQUEUE_STOPPED );
}

TEST( SIREventGoogleTest, RegistryRejectsDuplicatesAndStaleGenerations )
{
	SIREventRegistry registry;
	sirEventHandle_t first{};
	ASSERT_TRUE( registry.registerEvent( 90, "test.event", sizeof( uint64_t ), alignof( uint64_t ), first ) );
	sirEventHandle_t duplicate{};
	EXPECT_FALSE( registry.registerEvent( 90, "duplicate", 4, 4, duplicate ) );
	EXPECT_FALSE( registry.registerEvent( 91, "test.event", 4, 4, duplicate ) );
	EXPECT_TRUE( registry.unregisterEvent( first ) );
	EXPECT_FALSE( registry.unregisterEvent( first ) );
	sirEventHandle_t second{};
	ASSERT_TRUE( registry.registerEvent( 90, "test.event", sizeof( uint64_t ), alignof( uint64_t ), second ) );
	EXPECT_TRUE( second.nGeneration != first.nGeneration );
	sirRegisteredEvent_t descriptor{};
	EXPECT_FALSE( registry.getDescriptor( first, descriptor ) );
	EXPECT_TRUE( registry.getDescriptor( second, descriptor ) );
}

TEST( SIREventGoogleTest, ZeroDelaySchedulerDispatchesSynchronously )
{
	SIREventScheduler<int, 2> scheduler;
	int nSum = 0;
	sirEventScheduleHandle_t handle{};
	auto callback = []( void *pContext, const int &value ) noexcept { *( ( int * )pContext ) += value; };
	EXPECT_TRUE( scheduler.schedule( &nSum, callback, 7, 10, 0, 0, handle ) );
	EXPECT_EQ( nSum, 7 );
	EXPECT_EQ( scheduler.pendingCount(), 0U );
	EXPECT_FALSE( scheduler.cancel( handle ) );
}

TEST( SIREventGoogleTest, SchedulerExhaustedTimestampDoesNotLoop )
{
	SIREventScheduler<int, 2> scheduler;
	int nSum = 0;
	sirEventScheduleHandle_t handle{};
	auto callback = []( void *pContext, const int &value ) noexcept { *( ( int * )pContext ) += value; };
	ASSERT_TRUE( scheduler.schedule( &nSum, callback, 1, UINT64_MAX - 3, 1, 10, handle ) );
	EXPECT_EQ( scheduler.pump( UINT64_MAX ), 1U );
	EXPECT_EQ( scheduler.pendingCount(), 0U );
	EXPECT_EQ( nSum, 1 );
}

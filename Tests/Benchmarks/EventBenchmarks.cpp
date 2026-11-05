#include <Core/Events/SIREvent.h>
#include <Core/Events/SIREventStream.h>
#include <Core/Events/SIRSPSCEventRing.h>
#include <benchmark/benchmark.h>

static void SIRBenchmarkEventCallback( void *pContext, const uint32_t& payload ) noexcept
{
	*( ( uint64_t * )pContext ) += payload;
}

static void SIRBenchmarkTypedPublish( benchmark::State& state )
{
	SIREvent<uint32_t, 32> event;
	uint64_t nTotal = 0;
	for ( int i = 0; i < state.range( 0 ); ++i ) {
		const sirSubscriptionHandle_t handle = event.subscribe( &nTotal, SIRBenchmarkEventCallback );
		if ( !handle.isValid() ) {
			state.SkipWithError( "listener registration failed" );
			return;
		}
	}
	for ( auto _ : state ) {
		( void )_;
		event.publish( 1 );
		benchmark::DoNotOptimize( nTotal );
	}
	benchmark::DoNotOptimize( nTotal );
}
BENCHMARK( SIRBenchmarkTypedPublish )->Arg( 0 )->Arg( 1 )->Arg( 4 )->Arg( 8 )->Arg( 16 )->Arg( 32 );

static void SIRBenchmarkDirectCall( benchmark::State& state )
{
	uint64_t nTotal = 0;
	for ( auto _ : state ) {
		( void )_;
		SIRBenchmarkEventCallback( &nTotal, 1U );
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkDirectCall );

static void SIRBenchmarkStreamAppendDrain( benchmark::State& state )
{
	SIREventStream<uint32_t, 256> stream;
	uint64_t nTotal = 0;
	for ( auto _ : state ) {
		( void )_;
		if ( !stream.append( 1 ) ) {
			state.SkipWithError( "stream full" );
			break;
		}
		stream.drain( &nTotal, SIRBenchmarkEventCallback );
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkStreamAppendDrain );

static void SIRBenchmarkRingRoundTrip( benchmark::State& state )
{
	SIRSPSCEventRing<uint32_t, 256> ring;
	uint32_t nValue = 0;
	for ( auto _ : state ) {
		( void )_;
		benchmark::DoNotOptimize( ring.tryEnqueue( 1 ) );
		benchmark::DoNotOptimize( ring.tryDequeue( nValue ) );
		benchmark::DoNotOptimize( nValue );
	}
}
BENCHMARK( SIRBenchmarkRingRoundTrip );

// Compare sealed, immutable dispatch with the dynamic subscription hot path.
#include <Core/Events/SIREventMailbox.h>
#include <Core/Events/SIRMPSCEventQueue.h>
#include <Core/Events/SIRStaticEvent.h>
#include <Core/Events/SIREventTimingWheel.h>
#include <Core/Events/SIREventScheduler.h>

static void SIRBenchmarkStaticTypedPublish( benchmark::State& state )
{
	SIRStaticEvent<uint32_t, 32> event;
	uint64_t nTotal = 0;
	for ( int i = 0; i < state.range( 0 ); ++i ) {
		if ( !event.subscribe( &nTotal, SIRBenchmarkEventCallback ) ) {
			state.SkipWithError( "static event listener registration failed" );
			return;
		}
	}
	event.seal();
	for ( auto _ : state ) {
		( void )_;
		benchmark::DoNotOptimize( event.publish( 1 ) );
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkStaticTypedPublish )->Arg( 1 )->Arg( 8 )->Arg( 32 );

static void SIRBenchmarkSpscBatchRoundTrip( benchmark::State& state )
{
	SIRSPSCEventRing<uint32_t, 256> ring;
	uint32_t szInput[128]{};
	uint32_t szOutput[128]{};
	const uint32_t nBatch = ( uint32_t )state.range( 0 );
	for ( uint32_t i = 0; i < nBatch; ++i ) {
		szInput[i] = i + 1;
	}
	for ( auto _ : state ) {
		( void )_;
		if ( ring.enqueueBatch( szInput, nBatch ) != nBatch || ring.dequeueBatch( szOutput, nBatch ) != nBatch ) {
			state.SkipWithError( "SPSC batch incomplete" );
			break;
		}
		benchmark::DoNotOptimize( szOutput[0] );
	}
	state.SetItemsProcessed( state.iterations() * nBatch );
}
BENCHMARK( SIRBenchmarkSpscBatchRoundTrip )->Arg( 16 )->Arg( 64 )->Arg( 128 );

static void SIRBenchmarkMpscRoundTrip( benchmark::State& state )
{
	SIRMPSCEventQueue<uint32_t, 256> ingress;
	uint32_t nOutput = 0;
	for ( auto _ : state ) {
		( void )_;
		if ( ingress.tryEnqueue( 3 ) != SIR_EVENT_ENQUEUE_OK || !ingress.tryDequeue( nOutput ) ) {
			state.SkipWithError( "MPSC roundtrip failed" );
			break;
		}
		benchmark::DoNotOptimize( nOutput );
	}
}
BENCHMARK( SIRBenchmarkMpscRoundTrip );

static void SIRBenchmarkMailboxBatchPump( benchmark::State& state )
{
	SIREventMailbox<uint32_t, 2, 256> mailbox;
	const sirEventProducerHandle_t first = mailbox.registerProducer();
	const sirEventProducerHandle_t second = mailbox.registerProducer();
	mailbox.seal();
	uint32_t szBatch[64]{};
	uint64_t nTotal = 0;
	for ( uint32_t i = 0; i < 64; ++i ) {
		szBatch[i] = 1;
	}
	for ( auto _ : state ) {
		( void )_;
		if ( mailbox.publishBatch( first, szBatch, 64 ) != 64 || mailbox.publishBatch( second, szBatch, 64 ) != 64 ||
		     mailbox.pump( [&nTotal]( const uint32_t& value ) noexcept { nTotal += value; }, 128, 64 ) != 128 ) {
			state.SkipWithError( "mailbox batch pump failed" );
			break;
		}
		benchmark::DoNotOptimize( nTotal );
	}
	state.SetItemsProcessed( state.iterations() * 128 );
}
BENCHMARK( SIRBenchmarkMailboxBatchPump );

static void SIRBenchmarkDenseHeapTimers( benchmark::State& state )
{
	SIREventScheduler<uint32_t, 64> scheduler;
	uint64_t nTotal = 0;
	uint64_t nTick = 0;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < 64; ++i ) {
			sirEventScheduleHandle_t handle{};
			if ( !scheduler.schedule( &nTotal, SIRBenchmarkEventCallback, 1, nTick, 1, 0, handle ) ) {
				state.SkipWithError( "heap exhausted" );
				return;
			}
		}
		if ( scheduler.pump( ++nTick ) != 64 ) {
			state.SkipWithError( "heap did not dispatch timers" );
			return;
		}
		benchmark::DoNotOptimize( nTotal );
	}
	state.SetItemsProcessed( state.iterations() * 64 );
}
BENCHMARK( SIRBenchmarkDenseHeapTimers );

static void SIRBenchmarkDenseWheelTimers( benchmark::State& state )
{
	SIREventTimingWheel<uint32_t, 64, 256> scheduler;
	uint64_t nTotal = 0;
	uint64_t nTick = 0;
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < 64; ++i ) {
			sirEventScheduleHandle_t handle{};
			if ( !scheduler.schedule( &nTotal, SIRBenchmarkEventCallback, 1, nTick, 1, 0, handle ) ) {
				state.SkipWithError( "wheel exhausted" );
				return;
			}
		}
		if ( scheduler.pump( ++nTick ) != 64 ) {
			state.SkipWithError( "wheel did not dispatch timers" );
			return;
		}
		benchmark::DoNotOptimize( nTotal );
	}
	state.SetItemsProcessed( state.iterations() * 64 );
}
BENCHMARK( SIRBenchmarkDenseWheelTimers );

// Per-event metadata-free storage, dirty-ID coalescing, borrowed spans, and
// sparse timers. Run against optimized release builds; compare repeated JSON
// samples to the original transport tests rather than enforcing noisy CI timeouts.
#include <Core/Events/SIRChannelEventQueue.h>
#include <Core/Events/SIRDirtyEventSet.h>
#include <Core/Events/SIRExternalEventQueue.h>
#include <Core/Events/SIREventPhaseQueue.h>

static void SIRBenchmarkChannelQueueDispatch( benchmark::State& state )
{
	SIRChannelEventQueue<uint32_t, 256> queue;
	uint64_t nTotal = 0;
	if ( !queue.bind( &nTotal, SIRBenchmarkEventCallback ) ) {
		state.SkipWithError( "channel bind failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		if ( !queue.enqueue( 1 ) || queue.pump( 1 ) != 1 ) {
			state.SkipWithError( "channel roundtrip failed" );
			break;
		}
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkChannelQueueDispatch );

static void SIRBenchmarkExternalQueueDispatch( benchmark::State& state )
{
	uint32_t szSlots[256];
	SIRExternalEventQueue<uint32_t> queue;
	uint64_t nTotal = 0;
	if ( !queue.init( szSlots, 256, &nTotal, SIRBenchmarkEventCallback ) ) {
		state.SkipWithError( "external channel binding failed" );
		return;
	}
	for ( auto _ : state ) {
		( void )_;
		if ( !queue.enqueue( 1 ) || queue.pump( 1 ) != 1 ) {
			state.SkipWithError( "external channel roundtrip failed" );
			break;
		}
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkExternalQueueDispatch );

static void SIRBenchmarkSpscSpanRoundTrip( benchmark::State& state )
{
	SIRSPSCEventRing<uint32_t, 256> ring;
	uint32_t szBatch[128];
	const uint32_t nBatch = ( uint32_t )state.range( 0 );
	for ( uint32_t i = 0; i < nBatch; ++i ) {
		szBatch[i] = i + 1;
	}
	uint64_t nSum = 0;
	for ( auto _ : state ) {
		( void )_;
		if ( ring.enqueueBatch( szBatch, nBatch ) != nBatch ) {
			state.SkipWithError( "SPSC span input truncated" );
			break;
		}
		if ( ring.drainSpans( [&nSum]( const uint32_t *pValues, uint32_t nCount ) noexcept {
			     for ( uint32_t i = 0; i < nCount; ++i ) {
				     nSum += pValues[i];
			     }
		     } ) != nBatch ) {
			state.SkipWithError( "SPSC span output truncated" );
			break;
		}
		benchmark::DoNotOptimize( nSum );
	}
	state.SetItemsProcessed( state.iterations() * nBatch );
}
BENCHMARK( SIRBenchmarkSpscSpanRoundTrip )->Arg( 16 )->Arg( 64 )->Arg( 128 );

static void SIRBenchmarkCoalescedDirtyIDs( benchmark::State& state )
{
	SIRDirtyEventSet<4096, false> dirty;
	uint64_t nDelivered = 0;
	auto callback = []( void *pContext, sirCompactEventRef_t ref ) noexcept { *( ( uint64_t * )pContext ) += ref.nSlot; };
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < 512; ++i ) {
			dirty.mark( ( i % 64 ) + 1 );
		}
		if ( dirty.drain( &nDelivered, callback ) != 64 ) {
			state.SkipWithError( "dirty ID coalescing failed" );
			break;
		}
		benchmark::DoNotOptimize( nDelivered );
	}
	state.SetItemsProcessed( state.iterations() * 512 );
}
BENCHMARK( SIRBenchmarkCoalescedDirtyIDs );

static void SIRBenchmarkSparseBitmapWheel( benchmark::State& state )
{
	SIREventTimingWheel<uint32_t, 64, 256> wheel;
	uint64_t nNow = 100000;
	uint64_t nTotal = 0;
	for ( auto _ : state ) {
		( void )_;
		sirEventScheduleHandle_t handle{};
		if ( !wheel.schedule( &nTotal, SIRBenchmarkEventCallback, 1, nNow, 200, 0, handle ) ) {
			state.SkipWithError( "bitmap wheel scheduling failed" );
			break;
		}
		nNow += 200;
		if ( wheel.pump( nNow ) != 1 ) {
			state.SkipWithError( "bitmap wheel dispatch failed" );
			break;
		}
		benchmark::DoNotOptimize( nTotal );
	}
}
BENCHMARK( SIRBenchmarkSparseBitmapWheel );

static void SIRBenchmarkPhaseBudget( benchmark::State& state )
{
	SIRCompactPhaseQueue<uint32_t, 2, 256> queue;
	uint64_t nTotal = 0;
	auto callback = []( void *pContext, sirCompactEventRef_t, const uint32_t& payload ) noexcept {
		*( ( uint64_t * )pContext ) += payload;
	};
	for ( auto _ : state ) {
		( void )_;
		for ( uint32_t i = 0; i < 64; ++i ) {
			if ( !queue.enqueue( 0, 1 ) ) {
				state.SkipWithError( "phase queue exhausted" );
				return;
			}
		}
		if ( queue.pumpPhase( 0, &nTotal, callback, nullptr, 64 ) != 64 ) {
			state.SkipWithError( "phase budget mismatch" );
			break;
		}
		benchmark::DoNotOptimize( nTotal );
	}
	state.SetItemsProcessed( state.iterations() * 64 );
}
BENCHMARK( SIRBenchmarkPhaseBudget );

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
#include <Core/SIRApplication.h>
#include <Core/Scheduling/SIRPublishedState.h>
#include <Core/Scheduling/SIRSubsystemCommandQueue.h>
#include <Core/Scheduling/SIRTimelineCoordinator.h>
#include <Core/Scheduling/SIRSimulationSchedule.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>

#define SIR_FRAME_CHECK( bCondition )                                                                                                      \
	do {                                                                                                                                   \
		if ( !( bCondition ) ) {                                                                                                           \
			fprintf( stderr, "frame foundation failed %s:%d: %s\n", __FILE__, __LINE__, #bCondition );                                     \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirTestSnapshot_s
{
	uint64_t nTick;
	uint64_t nMirror;
} sirTestSnapshot_t;

static void testTimeline( void )
{
	SIRTimelineCoordinator timeline;
	SIR_FRAME_CHECK( !timeline.init( { 0, 2, 2 } ) );
	SIR_FRAME_CHECK( timeline.init( { 10000000, 2, 2 } ) );
	SIR_FRAME_CHECK( !timeline.init( { 10000000, 2, 2 } ) );
	SIR_FRAME_CHECK( timeline.beginFrame( 100, 25000000 ) );
	SIR_FRAME_CHECK( !timeline.beginFrame( 101, 0 ) );
	SIR_FRAME_CHECK( timeline.canAdvanceFixed() );
	SIR_FRAME_CHECK( timeline.completeFixedStep() );
	SIR_FRAME_CHECK( timeline.completeFixedStep() );
	SIR_FRAME_CHECK( !timeline.canAdvanceFixed() );
	SIR_FRAME_CHECK( timeline.publishFrame() );
	SIR_FRAME_CHECK( timeline.getFrame() == 1 && timeline.getSimulationTick() == 2 );
	sirTimelineSnapshot_t snapshot = {};
	SIR_FRAME_CHECK( timeline.getSnapshot( &snapshot ) && snapshot.nDroppedTimeNs == 5000000 );
	SIR_FRAME_CHECK( snapshot.nPublicationRevision == 1 && snapshot.fInterpolationAlpha == 0.0 );
	SIR_FRAME_CHECK( timeline.beginFrame( 200, 10000000 ) );
	SIR_FRAME_CHECK( timeline.publishFrame() );
	SIR_FRAME_CHECK( !timeline.beginFrame( 300, 10000000 ) );
	SIR_FRAME_CHECK( !timeline.acknowledgePresentedFrame( 3 ) );
	SIR_FRAME_CHECK( timeline.acknowledgePresentedFrame( 1 ) );
	SIR_FRAME_CHECK( timeline.beginFrame( 300, 10000000 ) );
	timeline.cancelFrame();
	SIR_FRAME_CHECK( !timeline.isFrameOpen() );
	SIR_FRAME_CHECK( timeline.getFrame() == 2 );
	SIR_FRAME_CHECK( timeline.getSnapshot( &snapshot ) && snapshot.nFrameTimeNs == 200 && snapshot.nDroppedTimeNs == 5000000 );
}

static void testCommandQueues( void )
{
	SIRSubsystemCommandQueue<uint32_t, 4> queue;
	SIR_FRAME_CHECK( queue.tryEnqueue( 10, 10, 100 ) );
	SIR_FRAME_CHECK( queue.tryEnqueue( 11, 1, 101 ) );
	SIR_FRAME_CHECK( queue.tryEnqueue( 12, 3, 102 ) );
	SIR_FRAME_CHECK( queue.tryEnqueue( 13, 2, 103 ) );
	SIR_FRAME_CHECK( !queue.tryEnqueue( 14, 1, 104 ) );

	sirSubsystemCommand_s<uint32_t> results[2] = {};
	SIR_FRAME_CHECK( queue.drainReady( 2, results, 2 ) == 2 );
	SIR_FRAME_CHECK( results[0].value == 11 && results[1].value == 13 );
	SIR_FRAME_CHECK( results[0].nSequence == 2 && results[1].nSequence == 4 );
	SIR_FRAME_CHECK( queue.getPendingCount() == 2 );
	SIR_FRAME_CHECK( queue.drainReady( 3, results, 2 ) == 1 && results[0].value == 12 );
	SIR_FRAME_CHECK( queue.drainReady( 9, results, 2 ) == 0 );
	SIR_FRAME_CHECK( queue.drainReady( 10, results, 2 ) == 1 && results[0].value == 10 );
	SIR_FRAME_CHECK( queue.getPendingCount() == 0 );
}

typedef struct sirQueueProducerContext_s
{
	SIRSubsystemCommandQueue<uint32_t, 64> *pQueue;
	std::atomic<uint32_t> *pFinished;
	uint32_t nStart;
} sirQueueProducerContext_t;

static void queueProducerEntry( void *pContext )
{
	sirQueueProducerContext_t *pProducer = ( sirQueueProducerContext_t * )pContext;
	for ( uint32_t iCommand = 0; iCommand < 200; ++iCommand ) {
		while ( !pProducer->pQueue->tryEnqueue( pProducer->nStart + iCommand, 0, iCommand ) ) {
			std::this_thread::yield();
		}
	}
	pProducer->pFinished->fetch_add( 1, std::memory_order_release );
}

static void testConcurrentCommands( void )
{
	SIRSubsystemCommandQueue<uint32_t, 64> queue;
	std::atomic<uint32_t> nFinished( 0 );
	sirQueueProducerContext_t producers[] = { { &queue, &nFinished, 0 }, { &queue, &nFinished, 200 } };
	SIRThread threads[2];
	for ( uint32_t iThread = 0; iThread < 2; ++iThread ) {
		SIR_FRAME_CHECK( threads[iThread].start( queueProducerEntry, &producers[iThread] ) );
	}
	bool szSeen[400] = {};
	uint64_t nLastSequence = 0;
	uint32_t nTotal = 0;
	for ( uint32_t nAttempt = 0; nAttempt < 100000 && nTotal < 400; ++nAttempt ) {
		sirSubsystemCommand_s<uint32_t> results[32] = {};
		const uint32_t nDrained = queue.drainReady( 0, results, 32 );
		for ( uint32_t iResult = 0; iResult < nDrained; ++iResult ) {
			SIR_FRAME_CHECK( results[iResult].value < 400 && !szSeen[results[iResult].value] );
			SIR_FRAME_CHECK( results[iResult].nSequence > nLastSequence );
			nLastSequence = results[iResult].nSequence;
			szSeen[results[iResult].value] = true;
			++nTotal;
		}
		if ( nDrained == 0 ) {
			std::this_thread::yield();
		}
	}
	for ( uint32_t iThread = 0; iThread < 2; ++iThread ) {
		SIR_FRAME_CHECK( threads[iThread].join() );
	}
	SIR_FRAME_CHECK( nFinished.load() == 2 && nTotal == 400 && queue.getPendingCount() == 0 );
}

typedef struct sirSnapshotReader_s
{
	SIRPublishedState<sirTestSnapshot_t> *pState;
	std::atomic<bool> *pStop;
	std::atomic<uint32_t> *pErrors;
} sirSnapshotReader_t;

static void snapshotReadEntry( void *pContext )
{
	sirSnapshotReader_t *pReader = ( sirSnapshotReader_t * )pContext;
	while ( !pReader->pStop->load( std::memory_order_acquire ) ) {
		SIRStateReadLease<sirTestSnapshot_t, 3> lease;
		if ( !lease.acquire( *pReader->pState ) ) {
			std::this_thread::yield();
			continue;
		}
		const sirTestSnapshot_t *pValue = lease.get();
		if ( pValue == nullptr || pValue->nMirror != pValue->nTick * 3 ) {
			pReader->pErrors->fetch_add( 1, std::memory_order_relaxed );
		}
		lease.release();
	}
}

static void testSnapshots( void )
{
	SIRPublishedState<sirTestSnapshot_t> state;
	SIRPublishedState<sirTestSnapshot_t, 4> larger;
	sirTestSnapshot_t *pLarger = nullptr;
	SIR_FRAME_CHECK( larger.beginWrite( &pLarger ) );
	pLarger->nTick = 42;
	pLarger->nMirror = 126;
	SIR_FRAME_CHECK( larger.commitWrite() );
	SIRStateReadLease<sirTestSnapshot_t, 4> largerReader;
	SIR_FRAME_CHECK( largerReader.acquire( larger ) && largerReader.get()->nTick == 42 );
	largerReader.release();
	SIRStateReadLease<sirTestSnapshot_t, 3> first;
	SIR_FRAME_CHECK( !first.acquire( state ) );
	sirTestSnapshot_t *pWritable = nullptr;
	SIR_FRAME_CHECK( state.beginWrite( &pWritable ) && pWritable != nullptr );
	pWritable->nTick = 1;
	pWritable->nMirror = 3;
	SIR_FRAME_CHECK( state.commitWrite() && first.acquire( state ) );
	SIR_FRAME_CHECK( first.getRevision() == 1 && first.get()->nTick == 1 );

	SIRStateReadLease<sirTestSnapshot_t, 3> second;
	SIR_FRAME_CHECK( state.beginWrite( &pWritable ) );
	pWritable->nTick = 2;
	pWritable->nMirror = 6;
	SIR_FRAME_CHECK( state.commitWrite() && second.acquire( state ) );
	SIR_FRAME_CHECK( first.get()->nTick == 1 && second.get()->nTick == 2 );
	SIR_FRAME_CHECK( state.beginWrite( &pWritable ) );
	pWritable->nTick = 3;
	pWritable->nMirror = 9;
	SIR_FRAME_CHECK( state.commitWrite() );
	SIR_FRAME_CHECK( !state.beginWrite( &pWritable ) );
	SIR_FRAME_CHECK( first.get()->nTick == 1 && second.get()->nTick == 2 );
	first.release();
	SIR_FRAME_CHECK( state.beginWrite( &pWritable ) );
	state.cancelWrite();
	SIR_FRAME_CHECK( state.beginWrite( &pWritable ) );
	pWritable->nTick = 4;
	pWritable->nMirror = 12;
	SIR_FRAME_CHECK( state.commitWrite() );
	SIR_FRAME_CHECK( second.get()->nTick == 2 );
	second.release();

	std::atomic<bool> bStop( false );
	std::atomic<uint32_t> nErrors( 0 );
	sirSnapshotReader_t reader = { &state, &bStop, &nErrors };
	SIRThread threads[4];
	for ( uint32_t i = 0; i < 4; ++i ) {
		SIR_FRAME_CHECK( threads[i].start( snapshotReadEntry, &reader ) );
	}
	uint32_t nPublished = 4;
	for ( uint32_t nAttempt = 0; nAttempt < 30000 && nPublished < 2000; ++nAttempt ) {
		if ( !state.beginWrite( &pWritable ) ) {
			std::this_thread::yield();
			continue;
		}
		++nPublished;
		pWritable->nTick = nPublished;
		pWritable->nMirror = nPublished * 3;
		SIR_FRAME_CHECK( state.commitWrite() );
	}
	bStop.store( true, std::memory_order_release );
	for ( uint32_t i = 0; i < 4; ++i ) {
		SIR_FRAME_CHECK( threads[i].join() );
	}
	SIR_FRAME_CHECK( nErrors.load() == 0 && nPublished > 100 );
}

class SIRFrameTestSystem final : public SIREngineSystem
{
public:
	SIRFrameTestSystem( uint32_t nId, std::atomic<uint32_t> *pOrder, std::atomic<uint32_t> *pFailures ) noexcept
	    : m_nId( nId ), m_pOrder( pOrder ), m_pFailures( pFailures ), m_nUpdates( 0 ), m_nFixed( 0 )
	{
	}
	sirSystemResult_t initialize( void ) noexcept override
	{
		return SIR_SYSTEM_OK;
	}
	void update( float ) noexcept override
	{
		const uint32_t nObserved = m_pOrder->fetch_add( 1 );
		if ( nObserved != m_nId ) {
			m_pFailures->fetch_add( 1 );
		}
		++m_nUpdates;
	}
	void fixedUpdate( float ) noexcept override
	{
		++m_nFixed;
	}
	sirSystemResult_t shutdown( void ) noexcept override
	{
		return SIR_SYSTEM_OK;
	}
	uint32_t getUpdates( void ) const noexcept
	{
		return m_nUpdates;
	}
	uint32_t getFixed( void ) const noexcept
	{
		return m_nFixed;
	}

private:
	uint32_t m_nId;
	std::atomic<uint32_t> *m_pOrder;
	std::atomic<uint32_t> *m_pFailures;
	uint32_t m_nUpdates;
	uint32_t m_nFixed;
};

static void testTaskedApplication( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 64 };
	SIR_FRAME_CHECK( memory.init( config ) );
	SIRTaskScheduler scheduler;
	SIR_FRAME_CHECK( scheduler.init( &memory, { 64, 2, 256 } ) );
	SIRSimulationSchedule schedule;
	SIR_FRAME_CHECK( schedule.init( &memory, &scheduler, { 16, 128, 256 } ) );
	SIRTimelineCoordinator timeline;
	SIR_FRAME_CHECK( timeline.init( { 10000000, 4, 0 } ) );

	std::atomic<uint32_t> nOrder( 0 );
	std::atomic<uint32_t> nErrors( 0 );
	SIRFrameTestSystem a( 0, &nOrder, &nErrors );
	SIRFrameTestSystem b( 1, &nOrder, &nErrors );
	SIRFrameTestSystem c( 2, &nOrder, &nErrors );
	SIRFrameTestSystem d( 3, &nOrder, &nErrors );
	SIRApplication application;
	SIR_FRAME_CHECK( application.attachTaskSchedule( schedule ) && application.attachTimeline( timeline ) );
	SIR_FRAME_CHECK( application.registerSystem( &a ) && application.registerSystem( &b ) );
	SIR_FRAME_CHECK( application.registerSystem( &c ) && application.registerSystem( &d ) );
	const sirTaskGraphAccess_t write = { { 100, 5 }, 0, 64, SIR_TASK_GRAPH_WRITE };
	const sirTaskGraphAccess_t read = { { 100, 5 }, 0, 64, SIR_TASK_GRAPH_READ };
	SIR_FRAME_CHECK( application.configureScheduledSystem( 1, SIR_TASK_EXEC_CPU, &write, 1 ) );
	SIR_FRAME_CHECK( application.configureScheduledSystem( 2, SIR_TASK_EXEC_CPU, &read, 1 ) );
	SIR_FRAME_CHECK( application.configureScheduledPriority( 1, SIR_TASK_PRIORITY_CRITICAL ) );
	SIR_FRAME_CHECK( application.addScheduledDependency( 1, 2 ) );
	SIR_FRAME_CHECK( !application.addScheduledDependency( 0, 0 ) );
	SIR_FRAME_CHECK( !application.configureScheduledSystem( 0, SIR_TASK_EXEC_AUDIO, nullptr, 0 ) );
	SIR_FRAME_CHECK( application.initialize() );
	SIR_FRAME_CHECK( application.beginScheduledTick( 0.020f, false ) );
	SIR_FRAME_CHECK( !application.beginScheduledTick( 0.020f, false ) );
	SIR_FRAME_CHECK( application.shutdown() == SIR_SYSTEM_BUSY );
	bool bComplete = false;
	for ( uint32_t iPoll = 0; iPoll < 100000; ++iPoll ) {
		const sirAppScheduledState_t eState = application.pollScheduledTick();
		if ( eState == SIR_APP_SCHEDULE_SUCCEEDED ) {
			bComplete = true;
			break;
		}
		SIR_FRAME_CHECK( eState == SIR_APP_SCHEDULE_PENDING );
		std::this_thread::yield();
	}
	SIR_FRAME_CHECK( bComplete && nOrder.load() == 4 && nErrors.load() == 0 );
	SIR_FRAME_CHECK( application.getTickCount() == 1 && timeline.getFrame() == 1 );
	SIR_FRAME_CHECK( application.beginScheduledTick( 0.01f, true ) );
	bComplete = false;
	for ( uint32_t iPoll = 0; iPoll < 100000; ++iPoll ) {
		const sirAppScheduledState_t eState = application.pollScheduledTick();
		if ( eState == SIR_APP_SCHEDULE_SUCCEEDED ) {
			bComplete = true;
			break;
		}
		SIR_FRAME_CHECK( eState == SIR_APP_SCHEDULE_PENDING );
		std::this_thread::yield();
	}
	SIR_FRAME_CHECK( bComplete && timeline.getSimulationTick() == 1 );
	SIR_FRAME_CHECK( a.getFixed() == 1 && b.getFixed() == 1 && c.getFixed() == 1 && d.getFixed() == 1 );
	SIR_FRAME_CHECK( a.getUpdates() == 1 && b.getUpdates() == 1 && c.getUpdates() == 1 && d.getUpdates() == 1 );
	SIR_FRAME_CHECK( application.shutdown() == SIR_SYSTEM_OK );
	SIR_FRAME_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirDeferredProviderContext_s
{
	sirTaskHandle_t handle;
	uint32_t nCalls;
} sirDeferredProviderContext_t;

static sirTaskResult_t deferredPhysicsJob( void *pContext, sirTaskHandle_t task ) noexcept
{
	sirDeferredProviderContext_t *pProvider = ( sirDeferredProviderContext_t * )pContext;
	pProvider->handle = task;
	++pProvider->nCalls;
	return SIR_TASK_RESULT_PENDING;
}

static sirTaskResult_t failingCpuJob( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_FAILURE;
}

static void testProviderPhases( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 64 };
	SIR_FRAME_CHECK( memory.init( config ) );
	SIRTaskScheduler scheduler;
	SIR_FRAME_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIR_FRAME_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_PHYSICS ) );
	SIRSimulationSchedule schedule;
	SIR_FRAME_CHECK( schedule.init( &memory, &scheduler, { 4, 4, 8 } ) );

	sirDeferredProviderContext_t provider = {};
	const sirSimulationPassDesc_t job = { { deferredPhysicsJob, &provider, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS }, nullptr, 0 };
	SIR_FRAME_CHECK( schedule.begin( &job, 1, nullptr, 0 ) );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_FRAME_CHECK( schedule.poll( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_PENDING );
	SIR_FRAME_CHECK( !schedule.finish() );
	SIR_FRAME_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 1 ) == 1 );
	SIR_FRAME_CHECK( provider.nCalls == 1 && scheduler.getStats().nAwaitingProvider == 1 );
	SIR_FRAME_CHECK( schedule.poll( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_PENDING );
	SIR_FRAME_CHECK( scheduler.completeProvider( provider.handle, SIR_TASK_RESULT_SUCCESS ) );
	SIR_FRAME_CHECK( schedule.poll( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_SUCCEEDED );
	SIR_FRAME_CHECK( schedule.finish() );

	const sirSimulationPassDesc_t failure = { { failingCpuJob, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, nullptr, 0 };
	SIR_FRAME_CHECK( schedule.begin( &failure, 1, nullptr, 0 ) );
	SIR_FRAME_CHECK( schedule.pumpCPU( 1 ) == 1 );
	SIR_FRAME_CHECK( schedule.poll( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_FAILED );
	SIR_FRAME_CHECK( schedule.finish() );
	SIR_FRAME_CHECK( scheduler.getActiveCount() == 0 && scheduler.shutdown() );
	memory.shutdown();
}

typedef struct sirParallelPassState_s
{
	std::atomic<uint32_t> nEntered;
} sirParallelPassState_t;

static sirTaskResult_t parallelCpuPass( void *pContext, sirTaskHandle_t ) noexcept
{
	sirParallelPassState_t *pShared = ( sirParallelPassState_t * )pContext;
	pShared->nEntered.fetch_add( 1, std::memory_order_acq_rel );
	for ( uint32_t iAttempt = 0; iAttempt < 1000000; ++iAttempt ) {
		if ( pShared->nEntered.load( std::memory_order_acquire ) >= 2 ) {
			return SIR_TASK_RESULT_SUCCESS;
		}
		std::this_thread::yield();
	}
	return SIR_TASK_RESULT_FAILURE;
}

static void testIndependentParallelJobs( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 64 };
	SIR_FRAME_CHECK( memory.init( config ) );
	SIRTaskScheduler scheduler;
	SIR_FRAME_CHECK( scheduler.init( &memory, { 8, 2, 8 } ) );
	SIRSimulationSchedule schedule;
	SIR_FRAME_CHECK( schedule.init( &memory, &scheduler, { 4, 8, 8 } ) );
	sirParallelPassState_t shared = {};
	const sirTaskGraphAccess_t accesses[2] = { { { 222, 2 }, 0, 16, SIR_TASK_GRAPH_WRITE }, { { 333, 3 }, 0, 16, SIR_TASK_GRAPH_WRITE } };
	const sirSimulationPassDesc_t jobs[2] = {
	    { { parallelCpuPass, &shared, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &accesses[0], 1 },
	    { { parallelCpuPass, &shared, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU }, &accesses[1], 1 } };
	SIR_FRAME_CHECK( schedule.begin( jobs, 2, nullptr, 0 ) );
	bool bComplete = false;
	for ( uint32_t iPoll = 0; iPoll < 100000; ++iPoll ) {
		sirTaskGraphExecutionSummary_t summary = {};
		SIR_FRAME_CHECK( schedule.poll( &summary ) );
		if ( summary.eState == SIR_TASK_GRAPH_EXEC_SUCCEEDED ) {
			bComplete = true;
			break;
		}
		SIR_FRAME_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_PENDING );
		std::this_thread::yield();
	}
	SIR_FRAME_CHECK( bComplete && shared.nEntered.load() == 2 );
	SIR_FRAME_CHECK( schedule.finish() && scheduler.shutdown() );
	memory.shutdown();
}

static void testCancelledApplicationPhase( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 64 };
	SIR_FRAME_CHECK( memory.init( config ) );
	SIRTaskScheduler scheduler;
	SIR_FRAME_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIRSimulationSchedule schedule;
	SIR_FRAME_CHECK( schedule.init( &memory, &scheduler, { 4, 4, 8 } ) );
	SIRTimelineCoordinator timeline;
	SIR_FRAME_CHECK( timeline.init( { 10000000, 2, 0 } ) );
	std::atomic<uint32_t> nOrder( 0 );
	std::atomic<uint32_t> nErrors( 0 );
	SIRFrameTestSystem physics( 0, &nOrder, &nErrors );
	SIRApplication application;
	SIR_FRAME_CHECK( application.attachTaskSchedule( schedule ) && application.attachTimeline( timeline ) );
	SIR_FRAME_CHECK( application.registerSystem( &physics ) );
	SIR_FRAME_CHECK( application.configureScheduledSystem( 0, SIR_TASK_EXEC_PHYSICS, nullptr, 0 ) );
	SIR_FRAME_CHECK( application.initialize() );
	SIR_FRAME_CHECK( application.beginScheduledTick( 0.02f, false ) );
	SIR_FRAME_CHECK( application.cancelScheduledTick() );
	SIR_FRAME_CHECK( application.pollScheduledTick() == SIR_APP_SCHEDULE_FAILED );
	SIR_FRAME_CHECK( application.getTickCount() == 0 && timeline.getFrame() == 0 );
	SIR_FRAME_CHECK( !timeline.isFrameOpen() && physics.getUpdates() == 0 );
	SIR_FRAME_CHECK( application.shutdown() == SIR_SYSTEM_OK );
	SIR_FRAME_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testDependencyRegistrationOrder( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 64 };
	SIR_FRAME_CHECK( memory.init( config ) );
	SIRTaskScheduler scheduler;
	SIR_FRAME_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIRSimulationSchedule schedule;
	SIR_FRAME_CHECK( schedule.init( &memory, &scheduler, { 4, 8, 8 } ) );
	std::atomic<uint32_t> nOrder( 0 );
	std::atomic<uint32_t> nErrors( 0 );
	SIRFrameTestSystem consumer( 1, &nOrder, &nErrors );
	SIRFrameTestSystem provider( 0, &nOrder, &nErrors );
	SIRApplication application;
	SIR_FRAME_CHECK( application.attachTaskSchedule( schedule ) );
	SIR_FRAME_CHECK( application.registerSystem( &consumer ) );
	SIR_FRAME_CHECK( application.registerSystem( &provider ) );
	SIR_FRAME_CHECK( application.addDependency( consumer, provider ) );
	SIR_FRAME_CHECK( application.initialize() );
	SIR_FRAME_CHECK( application.beginScheduledTick( 0.01f, false ) );
	bool bComplete = false;
	for ( uint32_t iPoll = 0; iPoll < 1000; ++iPoll ) {
		const sirAppScheduledState_t state = application.pollScheduledTick();
		if ( state == SIR_APP_SCHEDULE_SUCCEEDED ) {
			bComplete = true;
			break;
		}
		SIR_FRAME_CHECK( state == SIR_APP_SCHEDULE_PENDING );
	}
	SIR_FRAME_CHECK( bComplete && nErrors.load() == 0 && nOrder.load() == 2 );
	SIR_FRAME_CHECK( application.shutdown() == SIR_SYSTEM_OK );
	SIR_FRAME_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	testTimeline();
	testCommandQueues();
	testConcurrentCommands();
	testSnapshots();
	testTaskedApplication();
	testProviderPhases();
	testCancelledApplicationPhase();
	testIndependentParallelJobs();
	testDependencyRegistrationOrder();
	puts( "SIRFrameSchedulingTests PASS" );
	return 0;
}

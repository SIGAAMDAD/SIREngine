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

#include <Core/Jobs/SIRTaskGraph.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>

#define SIR_GRAPH_CHECK( condition )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Task graph test failed %s:%d: %s\n", __FILE__, __LINE__, #condition );                                       \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static sirTaskResult_t graphCount( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_SUCCESS;
}

static sirTaskResult_t graphFail( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_FAILURE;
}

static SIRMemoryManager *sirGraphMakeMemory( SIRMemoryManager *pMemory )
{
	const sirMemoryConfig_t memoryConfig = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	SIR_GRAPH_CHECK( pMemory->init( memoryConfig ) );
	return pMemory;
}

static bool graphHasEdge( const SIRTaskGraph &graph, uint32_t nBefore, uint32_t nAfter )
{
	for ( uint32_t iEdge = 0; iEdge < graph.getEdgeCount(); ++iEdge ) {
		sirTaskGraphEdge_t edge = {};
		SIR_GRAPH_CHECK( graph.getEdge( iEdge, &edge ) );
		if ( edge.nBefore == nBefore && edge.nAfter == nAfter ) {
			return true;
		}
	}
	return false;
}

static void testAccessHazards( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 16, 32, 64 } ) );
	std::atomic<uint32_t> nCounter( 0 );
	const sirTaskGraphPass_t pass = { graphCount, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphAccess_t accesses[] = { { { 1, 0 }, 0, 10, SIR_TASK_GRAPH_READ },   { { 1, 0 }, 0, 10, SIR_TASK_GRAPH_READ },
	                                          { { 1, 0 }, 0, 10, SIR_TASK_GRAPH_WRITE },  { { 1, 0 }, 0, 10, SIR_TASK_GRAPH_READ },
	                                          { { 1, 0 }, 20, 10, SIR_TASK_GRAPH_WRITE }, { { 1, 0 }, 0, 10, SIR_TASK_GRAPH_WRITE },
	                                          { { 2, 0 }, 0, 10, SIR_TASK_GRAPH_WRITE },  { { 1, 17 }, 0, 10, SIR_TASK_GRAPH_WRITE } };
	for ( uint32_t i = 0; i < 8; ++i ) {
		uint32_t nPass = UINT32_MAX;
		SIR_GRAPH_CHECK( graph.addPass( pass, &accesses[i], 1, &nPass ) );
		SIR_GRAPH_CHECK( nPass == i );
	}
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( !graphHasEdge( graph, 0, 1 ) );
	SIR_GRAPH_CHECK( graphHasEdge( graph, 0, 2 ) );
	SIR_GRAPH_CHECK( graphHasEdge( graph, 1, 2 ) );
	SIR_GRAPH_CHECK( graphHasEdge( graph, 2, 3 ) );
	SIR_GRAPH_CHECK( graphHasEdge( graph, 3, 5 ) );
	SIR_GRAPH_CHECK( !graphHasEdge( graph, 4, 5 ) );
	SIR_GRAPH_CHECK( !graphHasEdge( graph, 6, 5 ) );
	SIR_GRAPH_CHECK( !graphHasEdge( graph, 7, 5 ) );
	SIR_GRAPH_CHECK( !graphHasEdge( graph, 0, 7 ) );
	// Recompilation must produce the same dependency count.
	const uint32_t nEdges = graph.getEdgeCount();
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.getEdgeCount() == nEdges );
	SIR_GRAPH_CHECK( graph.reset() );
	SIR_GRAPH_CHECK( graph.getPassCount() == 0 );
	memory.shutdown();
}

static void testWriteFrontierAndCapacity( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskGraph graph;
	constexpr uint32_t N_WRITES = 256;
	SIR_GRAPH_CHECK( graph.init( &memory, { N_WRITES, N_WRITES, N_WRITES } ) );
	const sirTaskGraphPass_t pass = { graphCount, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphAccess_t access = { { 3, 0 }, 0, 2048, SIR_TASK_GRAPH_WRITE };
	for ( uint32_t i = 0; i < N_WRITES; ++i ) {
		uint32_t nIndex = UINT32_MAX;
		SIR_GRAPH_CHECK( graph.addPass( pass, &access, 1, &nIndex ) );
	}
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.getEdgeCount() == N_WRITES - 1 );
	SIR_GRAPH_CHECK( !graph.addPass( pass, &access, 1, nullptr ) );
	SIR_GRAPH_CHECK( graph.reset() );
	const sirTaskGraphAccess_t bad = { { 3, 0 }, UINT64_MAX, 2, SIR_TASK_GRAPH_WRITE };
	uint32_t nIndex = 0;
	SIR_GRAPH_CHECK( !graph.addPass( pass, &bad, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.getPassCount() == 0 );
	memory.shutdown();
}

static void testCyclesAndEdgeLimit( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 3, 3, 1 } ) );
	const sirTaskGraphPass_t pass = { graphCount, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphAccess_t read = { { 2, 0 }, 0, 8, SIR_TASK_GRAPH_READ };
	const sirTaskGraphAccess_t write = { { 2, 0 }, 0, 8, SIR_TASK_GRAPH_WRITE };
	uint32_t nIndex = 0;
	SIR_GRAPH_CHECK( graph.addPass( pass, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addPass( pass, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addPass( pass, &write, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_CAPACITY );
	SIR_GRAPH_CHECK( graph.reset() );
	SIR_GRAPH_CHECK( graph.addPass( pass, &write, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addPass( pass, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addDependency( 1, 0 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_CAPACITY );
	memory.shutdown();

	// A second graph with room for both opposite edges rejects the cycle.
	SIRMemoryManager second;
	sirGraphMakeMemory( &second );
	SIRTaskGraph cyclic;
	SIR_GRAPH_CHECK( cyclic.init( &second, { 3, 3, 8 } ) );
	SIR_GRAPH_CHECK( cyclic.addPass( pass, &write, 1, &nIndex ) );
	SIR_GRAPH_CHECK( cyclic.addPass( pass, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( cyclic.addDependency( 1, 0 ) );
	SIR_GRAPH_CHECK( cyclic.compile() == SIR_TASK_GRAPH_CYCLE );
	SIR_GRAPH_CHECK( cyclic.reset() );
	SIR_GRAPH_CHECK( cyclic.addPass( pass, &write, 1, &nIndex ) );
	SIR_GRAPH_CHECK( cyclic.addPass( pass, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( cyclic.compile() == SIR_TASK_GRAPH_OK );
	second.shutdown();
}

static void testAtomicAdmissionAndExecution( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	std::atomic<uint32_t> nCounter( 0 );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 8, 8, 16 } ) );
	const sirTaskGraphPass_t cpu = { graphCount, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphPass_t main = { graphCount, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN };
	const sirTaskGraphAccess_t read = { { 7, 0 }, 0, 32, SIR_TASK_GRAPH_READ };
	const sirTaskGraphAccess_t write = { { 7, 0 }, 0, 32, SIR_TASK_GRAPH_WRITE };
	uint32_t nIndex = 0;
	SIR_GRAPH_CHECK( graph.addPass( cpu, &write, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addPass( main, &read, 1, &nIndex ) );
	SIR_GRAPH_CHECK( graph.addPass( cpu, nullptr, 0, &nIndex ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );

	SIRTaskScheduler tiny;
	SIR_GRAPH_CHECK( tiny.init( &memory, { 2, 0, 4 } ) );
	SIR_GRAPH_CHECK( !graph.submit( &tiny ) );
	SIR_GRAPH_CHECK( tiny.getActiveCount() == 0 );
	SIR_GRAPH_CHECK( nCounter.load() == 0 );
	SIR_GRAPH_CHECK( tiny.shutdown() );

	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( !graph.reset() );
	SIR_GRAPH_CHECK( !graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( scheduler.pumpMain( 8 ) == 0 );
	SIR_GRAPH_CHECK( scheduler.pumpCPU( 8 ) == 2 );
	SIR_GRAPH_CHECK( scheduler.pumpMain( 8 ) == 1 );
	SIR_GRAPH_CHECK( nCounter.load() == 3 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( scheduler.getActiveCount() == 0 );
	SIR_GRAPH_CHECK( graph.reset() );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testFailurePropagationAndBatchValidation( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	const sirTaskBatchTask_t tasks[] = { { graphCount, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU },
	                                     { graphCount, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU } };
	const sirTaskBatchEdge_t invalid = { 1, 0 };
	sirTaskHandle_t handles[2] = { { 42, 77 }, { 43, 78 } };
	SIR_GRAPH_CHECK( !scheduler.submitBatch( tasks, 2, &invalid, 1, handles ) );
	SIR_GRAPH_CHECK( handles[0].nSlot == 42 && handles[1].nSlot == 43 );
	SIR_GRAPH_CHECK( scheduler.getActiveCount() == 0 );

	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 4, 4, 8 } ) );
	const sirTaskGraphPass_t first = { graphFail, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	std::atomic<uint32_t> nCount( 0 );
	const sirTaskGraphPass_t dependent = { graphCount, &nCount, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphAccess_t write = { { 5, 0 }, 0, 128, SIR_TASK_GRAPH_WRITE };
	const sirTaskGraphAccess_t read = { { 5, 0 }, 0, 128, SIR_TASK_GRAPH_READ };
	uint32_t nPass = UINT32_MAX;
	SIR_GRAPH_CHECK( graph.addPass( first, &write, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( dependent, &read, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( scheduler.pumpCPU( 5 ) == 1 );
	SIR_GRAPH_CHECK( nCount.load() == 0 );
	sirTaskHandle_t h = {};
	SIR_GRAPH_CHECK( graph.getSubmittedHandle( 1, &h ) );
	SIR_GRAPH_CHECK( scheduler.getState( h ) == SIR_TASK_CANCELLED );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static uint32_t graphNextRandom( uint32_t *pnSeed ) noexcept
{
	*pnSeed = ( *pnSeed * UINT32_C( 1664525 ) ) + UINT32_C( 1013904223 );
	return *pnSeed;
}

// Compare graph reachability against the conservative O(N^2) hazard oracle.
// Frontier pruning may remove direct edges but must preserve every ordering.
static void testRandomizedRangeHazards( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskGraph graph;
	constexpr uint32_t N_PASSES = 40;
	SIR_GRAPH_CHECK( graph.init( &memory, { N_PASSES, N_PASSES, N_PASSES * N_PASSES } ) );
	const sirTaskGraphPass_t pass = { graphCount, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	uint32_t nSeed = 0x38B4DA69;
	for ( uint32_t iCase = 0; iCase < 60; ++iCase ) {
		SIR_GRAPH_CHECK( graph.reset() );
		sirTaskGraphAccess_t accesses[N_PASSES] = {};
		bool bReachable[N_PASSES][N_PASSES] = {};
		for ( uint32_t i = 0; i < N_PASSES; ++i ) {
			accesses[i] = { { 1 + graphNextRandom( &nSeed ) % 6, 0 },
			                ( graphNextRandom( &nSeed ) % 12 ) * 8,
			                8 * ( 1 + graphNextRandom( &nSeed ) % 6 ),
			                ( graphNextRandom( &nSeed ) & 1 ) != 0 ? SIR_TASK_GRAPH_WRITE : SIR_TASK_GRAPH_READ };
			uint32_t nPass = UINT32_MAX;
			SIR_GRAPH_CHECK( graph.addPass( pass, accesses + i, 1, &nPass ) );
		}
		SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
		for ( uint32_t iEdge = 0; iEdge < graph.getEdgeCount(); ++iEdge ) {
			sirTaskGraphEdge_t edge = {};
			SIR_GRAPH_CHECK( graph.getEdge( iEdge, &edge ) );
			bReachable[edge.nBefore][edge.nAfter] = true;
		}
		for ( uint32_t iMid = 0; iMid < N_PASSES; ++iMid ) {
			for ( uint32_t iBefore = 0; iBefore < N_PASSES; ++iBefore ) {
				for ( uint32_t iAfter = 0; iAfter < N_PASSES; ++iAfter ) {
					bReachable[iBefore][iAfter] |= bReachable[iBefore][iMid] && bReachable[iMid][iAfter];
				}
			}
		}
		for ( uint32_t iBefore = 0; iBefore < N_PASSES; ++iBefore ) {
			for ( uint32_t iAfter = iBefore + 1; iAfter < N_PASSES; ++iAfter ) {
				const sirTaskGraphAccess_t &before = accesses[iBefore];
				const sirTaskGraphAccess_t &after = accesses[iAfter];
				const bool bOverlap = before.nOffset < after.nOffset + after.nBytes && after.nOffset < before.nOffset + before.nBytes;
				if ( before.keyResource.nLow == after.keyResource.nLow && before.keyResource.nHigh == after.keyResource.nHigh && bOverlap &&
				     ( before.eMode == SIR_TASK_GRAPH_WRITE || after.eMode == SIR_TASK_GRAPH_WRITE ) ) {
					SIR_GRAPH_CHECK( bReachable[iBefore][iAfter] );
				}
			}
		}
	}
	memory.shutdown();
}

static void testParallelGraphDispatch( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 128, 4, 256 } ) );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 96, 96, 256 } ) );
	std::atomic<uint32_t> nCounter( 0 );
	const sirTaskGraphPass_t pass = { graphCount, &nCounter, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	for ( uint32_t i = 0; i < 96; ++i ) {
		const sirTaskGraphAccess_t access = { { 100 + i % 8, 0 }, 0, 256, SIR_TASK_GRAPH_WRITE };
		uint32_t nPass = UINT32_MAX;
		SIR_GRAPH_CHECK( graph.addPass( pass, &access, 1, &nPass ) );
	}
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.getEdgeCount() == 88 );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	for ( uint32_t i = 88; i < 96; ++i ) {
		sirTaskHandle_t handle = {};
		SIR_GRAPH_CHECK( graph.getSubmittedHandle( i, &handle ) );
		SIR_GRAPH_CHECK( scheduler.waitTask( handle ) );
		SIR_GRAPH_CHECK( scheduler.getState( handle ) == SIR_TASK_COMPLETED );
	}
	SIR_GRAPH_CHECK( nCounter.load() == 96 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( scheduler.getActiveCount() == 0 );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static sirTaskResult_t graphPending( void *pContext, sirTaskHandle_t ) noexcept
{
	( ( std::atomic<uint32_t> * )pContext )->fetch_add( 1 );
	return SIR_TASK_RESULT_PENDING;
}

static void testGraphExecutionSummaryAndProviderLifetime( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 12, 0, 24 } ) );
	SIR_GRAPH_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 4, 4, 8 } ) );
	std::atomic<uint32_t> nCalls( 0 );
	uint32_t nPass = UINT32_MAX;
	const sirTaskGraphPass_t cpu = { graphCount, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphPass_t render = { graphPending, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER };
	const sirTaskGraphPass_t main = { graphCount, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_MAIN };
	const sirTaskGraphAccess_t write = { { 8, 12 }, 0, 32, SIR_TASK_GRAPH_WRITE };
	const sirTaskGraphAccess_t read = { { 8, 12 }, 0, 32, SIR_TASK_GRAPH_READ };
	SIR_GRAPH_CHECK( graph.addPass( cpu, &write, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( render, &read, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( main, &read, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.addDependency( 1, 2 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_GRAPH_CHECK( !graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( !graph.cancelExecution() );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.nPending == 3 && summary.nSucceeded == 0 );
	SIR_GRAPH_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_PENDING );
	SIR_GRAPH_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	SIR_GRAPH_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.nPending == 2 && summary.nSucceeded == 1 );
	SIR_GRAPH_CHECK( !graph.releaseSubmitted() );
	sirTaskHandle_t renderHandle = {};
	SIR_GRAPH_CHECK( graph.getSubmittedHandle( 1, &renderHandle ) );
	SIR_GRAPH_CHECK( scheduler.completeProvider( renderHandle, SIR_TASK_RESULT_SUCCESS ) );
	SIR_GRAPH_CHECK( scheduler.pumpMain( 1 ) == 1 );
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_SUCCEEDED );
	SIR_GRAPH_CHECK( summary.nPending == 0 && summary.nSucceeded == 3 );
	SIR_GRAPH_CHECK( nCalls.load() == 3 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( !graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( graph.reset() );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testGraphExecutionCancellationAndFailures( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 12, 0, 24 } ) );
	SIR_GRAPH_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_PHYSICS ) );
	SIRTaskGraph graph;
	SIR_GRAPH_CHECK( graph.init( &memory, { 4, 4, 8 } ) );
	std::atomic<uint32_t> nCalls( 0 );
	uint32_t nPass = UINT32_MAX;
	const sirTaskGraphPass_t physics = { graphPending, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_PHYSICS };
	const sirTaskGraphPass_t child = { graphCount, &nCalls, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	SIR_GRAPH_CHECK( graph.addPass( physics, nullptr, 0, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( child, nullptr, 0, &nPass ) );
	SIR_GRAPH_CHECK( graph.addDependency( 0, 1 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_PHYSICS, 1 ) == 1 );
	SIR_GRAPH_CHECK( graph.cancelExecution() );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_PENDING && summary.nPending == 1 );
	SIR_GRAPH_CHECK( summary.nCancelled == 1 );
	SIR_GRAPH_CHECK( !graph.releaseSubmitted() );
	sirTaskHandle_t task = {};
	SIR_GRAPH_CHECK( graph.getSubmittedHandle( 0, &task ) );
	SIR_GRAPH_CHECK( scheduler.completeProvider( task, SIR_TASK_RESULT_SUCCESS ) );
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_CANCELLED && summary.nCancelled == 2 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( graph.reset() );
	const sirTaskGraphPass_t failed = { graphFail, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	SIR_GRAPH_CHECK( graph.addPass( failed, nullptr, 0, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( child, nullptr, 0, &nPass ) );
	SIR_GRAPH_CHECK( graph.addDependency( 0, 1 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.eState == SIR_TASK_GRAPH_EXEC_FAILED );
	SIR_GRAPH_CHECK( summary.nFailed == 1 && summary.nCancelled == 1 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}


static void testGraphInputValidationAndInspection( void )
{
	SIRMemoryManager memory;
	SIRTaskGraph graph;
	const sirTaskGraphPass_t pass = { graphFail, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	const sirTaskGraphAccess_t access = { { 1, 0 }, 0, 16, SIR_TASK_GRAPH_READ };
	uint32_t nPass = UINT32_MAX;
	sirTaskGraphEdge_t edge = {};
	uint32_t nTopological = UINT32_MAX;
	SIR_GRAPH_CHECK( !graph.reset() );
	SIR_GRAPH_CHECK( !graph.addPass( pass, &access, 1, &nPass ) );
	SIR_GRAPH_CHECK( !graph.addDependency( 0, 1 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_INVALID );
	SIR_GRAPH_CHECK( !graph.getEdge( 0, &edge ) );
	SIR_GRAPH_CHECK( !graph.getTopologicalPass( 0, &nTopological ) );
	SIR_GRAPH_CHECK( !graph.submit( nullptr ) );
	SIR_GRAPH_CHECK( !graph.getSubmittedHandle( 0, nullptr ) );
	SIR_GRAPH_CHECK( !graph.queryExecution( nullptr ) );
	SIR_GRAPH_CHECK( !graph.cancelExecution() );
	SIR_GRAPH_CHECK( !graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( !graph.init( nullptr, { 2, 2, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, 2, 4 } ) );
	sirGraphMakeMemory( &memory );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 0, 2, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, 0, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, 2, 0 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { UINT32_MAX, 2, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, UINT32_MAX, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, 2, UINT32_MAX } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, UINT32_MAX / 2 + 1, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, UINT32_MAX / 2, 4 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 100000, 100000, 100000 } ) );
	SIR_GRAPH_CHECK( graph.init( &memory, { 2, 2, 1 } ) );
	SIR_GRAPH_CHECK( !graph.init( &memory, { 2, 2, 1 } ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_INVALID );
	SIR_GRAPH_CHECK( !graph.addPass( pass, nullptr, 1, &nPass ) );
	SIR_GRAPH_CHECK( !graph.addPass( pass, &access, 1, nullptr ) );
	sirTaskGraphPass_t invalidPass = pass;
	invalidPass.pFunction = nullptr;
	SIR_GRAPH_CHECK( !graph.addPass( invalidPass, &access, 1, &nPass ) );
	invalidPass = pass;
	invalidPass.eExecutor = SIR_TASK_EXEC_COUNT;
	SIR_GRAPH_CHECK( !graph.addPass( invalidPass, &access, 1, &nPass ) );
	invalidPass = pass;
	invalidPass.ePriority = ( sirTaskPriority_t )255;
	SIR_GRAPH_CHECK( !graph.addPass( invalidPass, &access, 1, &nPass ) );
	SIR_GRAPH_CHECK( !graph.addPass( pass, &access, 3, &nPass ) );
	sirTaskGraphAccess_t invalidAccess = access;
	invalidAccess.keyResource = { 0, 0 };
	SIR_GRAPH_CHECK( !graph.addPass( pass, &invalidAccess, 1, &nPass ) );
	invalidAccess = access;
	invalidAccess.nBytes = 0;
	SIR_GRAPH_CHECK( !graph.addPass( pass, &invalidAccess, 1, &nPass ) );
	invalidAccess = access;
	invalidAccess.nOffset = UINT64_MAX - 1;
	invalidAccess.nBytes = 3;
	SIR_GRAPH_CHECK( !graph.addPass( pass, &invalidAccess, 1, &nPass ) );
	invalidAccess = access;
	invalidAccess.eMode = ( sirTaskGraphAccessMode_t )255;
	SIR_GRAPH_CHECK( !graph.addPass( pass, &invalidAccess, 1, &nPass ) );
	SIR_GRAPH_CHECK( graph.addPass( pass, &access, 1, &nPass ) && nPass == 0 );
	SIR_GRAPH_CHECK( graph.addPass( pass, &access, 1, &nPass ) && nPass == 1 );
	SIR_GRAPH_CHECK( !graph.addPass( pass, &access, 1, &nPass ) );
	SIR_GRAPH_CHECK( !graph.addDependency( 0, 0 ) );
	SIR_GRAPH_CHECK( !graph.addDependency( 2, 1 ) );
	SIR_GRAPH_CHECK( !graph.addDependency( 0, 2 ) );
	SIR_GRAPH_CHECK( graph.addDependency( 0, 1 ) );
	SIR_GRAPH_CHECK( !graph.addDependency( 0, 1 ) );
	SIR_GRAPH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_GRAPH_CHECK( graph.getEdgeCount() == 1 );
	SIR_GRAPH_CHECK( !graph.getEdge( 1, &edge ) );
	SIR_GRAPH_CHECK( !graph.getEdge( 0, nullptr ) );
	SIR_GRAPH_CHECK( graph.getEdge( 0, &edge ) );
	SIR_GRAPH_CHECK( !graph.getTopologicalPass( 2, &nTopological ) );
	SIR_GRAPH_CHECK( !graph.getTopologicalPass( 0, nullptr ) );
	SIR_GRAPH_CHECK( graph.getTopologicalPass( 0, &nTopological ) );
	SIR_GRAPH_CHECK( !graph.submit( nullptr ) );
	SIRTaskScheduler scheduler;
	SIR_GRAPH_CHECK( scheduler.init( &memory, { 4, 0, 4 } ) );
	SIR_GRAPH_CHECK( !graph.getSubmittedHandle( 0, &edge.nBefore ) );
	SIR_GRAPH_CHECK( graph.submit( &scheduler ) );
	SIR_GRAPH_CHECK( !graph.submit( &scheduler ) );
	sirTaskHandle_t submitted = {};
	SIR_GRAPH_CHECK( !graph.getSubmittedHandle( 2, &submitted ) );
	SIR_GRAPH_CHECK( !graph.getSubmittedHandle( 0, nullptr ) );
	SIR_GRAPH_CHECK( graph.getSubmittedHandle( 0, &submitted ) );
	SIR_GRAPH_CHECK( graph.cancelExecution() );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_GRAPH_CHECK( graph.queryExecution( &summary ) );
	SIR_GRAPH_CHECK( summary.nCancelled == 2 );
	SIR_GRAPH_CHECK( graph.releaseSubmitted() );
	SIR_GRAPH_CHECK( !graph.getSubmittedHandle( 0, &submitted ) );
	SIR_GRAPH_CHECK( graph.reset() );
	SIR_GRAPH_CHECK( scheduler.shutdown() );
	memory.shutdown();
}

static void testBucketCollisionAndInteriorFrontierPruning( void )
{
	SIRMemoryManager memory;
	sirGraphMakeMemory( &memory );
	const sirTaskGraphPass_t pass = { graphFail, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	uint32_t nIndex = 0;
	{
		SIRTaskGraph collision;
		SIR_GRAPH_CHECK( collision.init( &memory, { 2, 2, 2 } ) );
		const sirTaskGraphAccess_t first = { { 4, 0 }, 0, 8, SIR_TASK_GRAPH_READ };
		const sirTaskGraphAccess_t second = { { 5, 0 }, 0, 8, SIR_TASK_GRAPH_READ };
		SIR_GRAPH_CHECK( collision.addPass( pass, &first, 1, &nIndex ) );
		SIR_GRAPH_CHECK( collision.addPass( pass, &second, 1, &nIndex ) );
		SIR_GRAPH_CHECK( collision.compile() == SIR_TASK_GRAPH_OK );
		SIR_GRAPH_CHECK( collision.getEdgeCount() == 0 );
	}
	{
		SIRTaskGraph frontier;
		SIR_GRAPH_CHECK( frontier.init( &memory, { 4, 4, 8 } ) );
		const sirTaskGraphAccess_t ranges[4] = {
		    { { 8, 0 }, 0, 8, SIR_TASK_GRAPH_READ },
		    { { 8, 0 }, 8, 8, SIR_TASK_GRAPH_READ },
		    { { 8, 0 }, 16, 8, SIR_TASK_GRAPH_READ },
		    { { 8, 0 }, 8, 8, SIR_TASK_GRAPH_WRITE }
		};
		for ( uint32_t i = 0; i < 4; ++i ) {
			SIR_GRAPH_CHECK( frontier.addPass( pass, ranges + i, 1, &nIndex ) );
		}
		SIR_GRAPH_CHECK( frontier.compile() == SIR_TASK_GRAPH_OK );
		SIR_GRAPH_CHECK( graphHasEdge( frontier, 1, 3 ) );
		SIR_GRAPH_CHECK( !graphHasEdge( frontier, 0, 3 ) );
		SIR_GRAPH_CHECK( !graphHasEdge( frontier, 2, 3 ) );
	}
	memory.shutdown();
}

int main( void )
{
	testAccessHazards();
	testWriteFrontierAndCapacity();
	testCyclesAndEdgeLimit();
	testAtomicAdmissionAndExecution();
	testFailurePropagationAndBatchValidation();
	testRandomizedRangeHazards();
	testParallelGraphDispatch();
	testGraphExecutionSummaryAndProviderLifetime();
	testGraphExecutionCancellationAndFailures();
	testGraphInputValidationAndInspection();
	testBucketCollisionAndInteriorFrontierPruning();
	return 0;
}

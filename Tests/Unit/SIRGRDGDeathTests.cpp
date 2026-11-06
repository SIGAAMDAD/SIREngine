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
#include <Core/Jobs/SIRTaskFenceTracker.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRVirtualMemory.h>
#include <Resources/SIRResourceFileLoad.h>
#include <Resources/SIRResourceGraph.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIR_DEATH_CHECK( expression )                                                                                                      \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			_exit( 70 );                                                                                                                   \
		}                                                                                                                                  \
	} while ( false )

// A death test must flush its own instrumented counters before terminating.
// Only an instrumented build needs to resolve the GCC profiling runtime.
#if defined( SIR_GRDG_GCOV_ENABLED )
extern "C" void __gcov_dump( void );
#endif

#if defined( __clang__ )
extern "C" int __llvm_profile_write_file( void ) __attribute__( ( weak ) );
extern "C" void __llvm_profile_set_filename( const char *pszName ) __attribute__( ( weak ) );
extern "C" void __llvm_profile_reset_counters( void ) __attribute__( ( weak ) );
#endif

extern "C" [[noreturn]] void __wrap_abort( void )
{
#if defined( SIR_GRDG_GCOV_ENABLED )
	__gcov_dump();
#endif
#if defined( __clang__ )
	if ( __llvm_profile_write_file != nullptr ) {
		( void )__llvm_profile_write_file();
	}
#endif
	_exit( 81 );
}

static void *deathAllocate( void *, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	void *pMemory = nullptr;
	const size_t nAligned = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	return nBytes != 0 && nBytes <= SIZE_MAX && posix_memalign( &pMemory, nAligned, ( size_t )nBytes ) == 0 ? pMemory : nullptr;
}

static void deathFree( void *, void *pMemory, uint64_t, uint32_t, uint32_t ) noexcept
{
	free( pMemory );
}

static bool deathRelease( void *, uint64_t ) noexcept
{
	return true;
}

static sirTaskResult_t deathProviderTask( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_PENDING;
}

typedef struct sirDeathPrematureCompletion_s
{
	SIRTaskScheduler *pScheduler;
	sirTaskHandle_t task;
} sirDeathPrematureCompletion_t;

static bool deathCompleteProviderEarly( void *pContext ) noexcept
{
	sirDeathPrematureCompletion_t *pRecord = ( sirDeathPrematureCompletion_t * )pContext;
	return pRecord->pScheduler->completeProvider( pRecord->task, SIR_TASK_RESULT_SUCCESS );
}

static void deathInitMemory( SIRMemoryManager& rMemory )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 8 };
	SIR_DEATH_CHECK( rMemory.init( config ) );
}

static SIRResourceGraphConfig deathGraphConfig( void ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, deathAllocate, deathFree };
	config.m_nMaxResources = 4;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 4096;
	return config;
}

static void deathActiveFileLoad( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIRResourceGraph graph( deathGraphConfig() );
	SIR_DEATH_CHECK( graph.isReady() );
	SIRResourceFileLoad loader( graph, scheduler );
	sirResourceFileLoadRequest_t request = {};
	request.keyResource = { 31, 17, 0 };
	request.keyContent = { 7, 9, 1, 0 };
	request.eKind = SIR_RESOURCE_KIND_BLOB;
	request.eMode = SIR_RESOURCE_FILE_READ_AT;
	request.nSchemaVersion = 1;
	request.nAlignment = 8;
	request.nFileBytes = 8;
	SIR_DEATH_CHECK( loader.begin( "sir_grdg_death_test.bin", request ) == SIR_RESOURCE_RESULT_SUCCESS );
}

static void deathLiveDerivedRegistry( void )
{
	SIRResourceGraph graph( deathGraphConfig() );
	SIR_DEATH_CHECK( graph.isReady() );
	SIRResourceGraphWriter *pWriter = graph.getWriter();
	SIR_DEATH_CHECK( pWriter != nullptr );
	SIRResourcePublication pub = { { 32, 17, 0 }, SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourceHandle resource = {};
	SIR_DEATH_CHECK( pWriter->publish( pub, &resource ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIRResourceDerivedConfig config = {};
	config.allocator = { nullptr, deathAllocate, deathFree };
	config.nMaxProviders = 1;
	config.nMaxRepresentations = 1;
	config.nMaxTrackedBytes = 1024;
	SIRResourceDerivedRegistry registry( graph, config );
	SIR_DEATH_CHECK( registry.isReady() );
	SIR_DEATH_CHECK( registry.registerProvider( 7, { nullptr, deathRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );
	{
		SIRResourceReadScope scope( graph );
		SIRResourceView view;
		SIR_DEATH_CHECK( graph.resolve( scope, resource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRDerivedResourceRegistration registration = {};
		registration.m_Key = { resource, 1, 7, 1, 0, 0 };
		registration.m_Accounting = { SIR_MEMORY_SPACE_DEVICE, 1, 32, 16, 1 };
		registration.m_nNativeToken = 42;
		SIRDerivedResourceHandle representation = {};
		SIR_DEATH_CHECK( registry.registerRepresentation( scope, view, registration, &representation ) == SIR_RESOURCE_RESULT_SUCCESS );
	}
}

static void deathPendingProvider( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIR_DEATH_CHECK( scheduler.bindProviderOwner( SIR_TASK_EXEC_RENDER ) );
	sirTaskHandle_t task = {};
	SIR_DEATH_CHECK( scheduler.submit( { deathProviderTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &task ) );
	SIR_DEATH_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	// The scheduler refuses destruction while the provider owns a pending task.
}

static void deathPendingFence( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIRTaskFenceTracker tracker;
	SIR_DEATH_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	sirTaskHandle_t task = {};
	SIR_DEATH_CHECK( scheduler.submit( { deathProviderTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &task ) );
	SIR_DEATH_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	SIR_DEATH_CHECK( tracker.track( task, 1 ) );
}

static void deathPrematureProviderCompletion( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 8, 0, 8 } ) );
	SIRTaskFenceTracker tracker;
	SIR_DEATH_CHECK( tracker.init( &memory, &scheduler, SIR_TASK_EXEC_RENDER, 2 ) );
	sirTaskHandle_t task = {};
	SIR_DEATH_CHECK( scheduler.submit( { deathProviderTask, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_RENDER }, &task ) );
	SIR_DEATH_CHECK( scheduler.pumpProvider( SIR_TASK_EXEC_RENDER, 1 ) == 1 );
	sirDeathPrematureCompletion_t context = { &scheduler, task };
	SIR_DEATH_CHECK( tracker.track( task, 1, deathCompleteProviderEarly, &context ) );
	uint32_t nAcknowledged = 0;
	SIR_DEATH_CHECK( !tracker.advanceCompleted( 1, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) );
	SIR_DEATH_CHECK( nAcknowledged == 0 && tracker.getPendingCount() == 1 );
	// A failed provider acknowledgement preserves the successful finalizer
	// and must not invoke it again on a subsequent drain attempt.
	SIR_DEATH_CHECK( !tracker.advanceCompleted( 1, SIR_TASK_RESULT_SUCCESS, &nAcknowledged ) );
	SIR_DEATH_CHECK( nAcknowledged == 0 && tracker.getPendingCount() == 1 );
	SIR_DEATH_CHECK( !tracker.untrackLast( task ) );
	// The tracker cannot finalize a provider slot completed by another owner.
	// Leaving it live must trigger the destructor's ownership guard.
}

static void deathGraphWithActiveReader( void )
{
	SIRResourceGraph *pGraph = new SIRResourceGraph( deathGraphConfig() );
	SIR_DEATH_CHECK( pGraph->isReady() );
	SIRResourceReadScope reader( *pGraph );
	SIR_DEATH_CHECK( reader.isActive() );
	delete pGraph;
}

static void deathGraphWithProviderPin( void )
{
	SIRResourceGraph *pGraph = new SIRResourceGraph( deathGraphConfig() );
	SIR_DEATH_CHECK( pGraph->isReady() );
	SIRResourceGraphWriter *pWriter = pGraph->getWriter();
	SIRResourcePublication pub = { { 33, 17, 0 }, SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
	SIRResourceHandle handle = {};
	SIR_DEATH_CHECK( pWriter->publish( pub, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
	{
		SIRResourceReadScope reader( *pGraph );
		SIRResourceView view;
		SIR_DEATH_CHECK( pGraph->resolve( reader, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourcePinHandle pin = {};
		SIR_DEATH_CHECK( pGraph->pinForProvider( reader, view, 1, &pin ) == SIR_RESOURCE_RESULT_SUCCESS );
	}
	delete pGraph;
}

static sirTaskResult_t deathGraphPass( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_SUCCESS;
}

static void deathSubmittedTaskGraph( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 4, 0, 4 } ) );
	SIRTaskGraph graph;
	SIR_DEATH_CHECK( graph.init( &memory, { 2, 1, 2 } ) );
	const sirTaskGraphPass_t pass = { deathGraphPass, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	uint32_t nPass = 0;
	SIR_DEATH_CHECK( graph.addPass( pass, nullptr, 0, &nPass ) );
	SIR_DEATH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_DEATH_CHECK( graph.submit( &scheduler ) );
}

static void deathExternallyReleasedTaskGraph( void )
{
	SIRMemoryManager memory;
	deathInitMemory( memory );
	SIRTaskScheduler scheduler;
	SIR_DEATH_CHECK( scheduler.init( &memory, { 4, 0, 4 } ) );
	SIRTaskGraph graph;
	SIR_DEATH_CHECK( graph.init( &memory, { 2, 1, 2 } ) );
	const sirTaskGraphPass_t pass = { deathGraphPass, nullptr, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	uint32_t nPass = UINT32_MAX;
	SIR_DEATH_CHECK( graph.addPass( pass, nullptr, 0, &nPass ) );
	SIR_DEATH_CHECK( graph.compile() == SIR_TASK_GRAPH_OK );
	SIR_DEATH_CHECK( graph.submit( &scheduler ) );
	SIR_DEATH_CHECK( scheduler.pumpCPU( 1 ) == 1 );
	sirTaskHandle_t task = {};
	SIR_DEATH_CHECK( graph.getSubmittedHandle( 0, &task ) );
	SIR_DEATH_CHECK( scheduler.releaseTask( task ) );
	sirTaskGraphExecutionSummary_t summary = {};
	SIR_DEATH_CHECK( !graph.queryExecution( &summary ) );
	SIR_DEATH_CHECK( !graph.cancelExecution() );
	SIR_DEATH_CHECK( !graph.releaseSubmitted() );
	// A submitted graph whose scheduler handle was externally recycled is
	// still an outstanding lifetime obligation, rejected by its destructor.
}

static void testExpectedAbort( void ( *pfnScenario )( void ) )
{
	const pid_t nChild = fork();
	SIR_DEATH_CHECK( nChild >= 0 );
	if ( nChild == 0 ) {
#if defined( __clang__ )
		const char *pszProfilePattern = getenv( "LLVM_PROFILE_FILE" );
		if ( pszProfilePattern != nullptr && __llvm_profile_set_filename != nullptr ) {
			char szChildProfile[1024];
			const int nLength =
			    snprintf( szChildProfile, sizeof( szChildProfile ), "%s.child.%ld.profraw", pszProfilePattern, ( long )getpid() );
			if ( nLength > 0 && ( size_t )nLength < sizeof( szChildProfile ) ) {
				__llvm_profile_set_filename( szChildProfile );
				if ( __llvm_profile_reset_counters != nullptr ) {
					__llvm_profile_reset_counters();
				}
			}
		}
#endif
		pfnScenario();
		_exit( 0 );
	}
	int nStatus = 0;
	SIR_DEATH_CHECK( waitpid( nChild, &nStatus, 0 ) == nChild );
	SIR_DEATH_CHECK( WIFEXITED( nStatus ) && WEXITSTATUS( nStatus ) == 81 );
}

int main( void )
{
	SIRPlatformFile::remove( "sir_grdg_death_test.bin" );
	SIRPlatformFile file;
	SIR_DEATH_CHECK( file.open( "sir_grdg_death_test.bin", SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE ) == SIR_FILE_OK );
	const char szData[8] = { 'd', 'e', 'a', 't', 'h', '0', '0', '1' };
	uint64_t nTransferred = 0;
	SIR_DEATH_CHECK( file.writeAt( 0, szData, sizeof( szData ), &nTransferred ) == SIR_FILE_OK && nTransferred == sizeof( szData ) );
	SIR_DEATH_CHECK( file.close() == SIR_FILE_OK );
	testExpectedAbort( deathActiveFileLoad );
	testExpectedAbort( deathLiveDerivedRegistry );
	testExpectedAbort( deathPendingProvider );
	testExpectedAbort( deathPendingFence );
	testExpectedAbort( deathPrematureProviderCompletion );
	testExpectedAbort( deathGraphWithActiveReader );
	testExpectedAbort( deathGraphWithProviderPin );
	testExpectedAbort( deathSubmittedTaskGraph );
	testExpectedAbort( deathExternallyReleasedTaskGraph );
	SIR_DEATH_CHECK( SIRPlatformFile::remove( "sir_grdg_death_test.bin" ) == SIR_FILE_OK );
	puts( "PASS: GRDG lifecycle destructor invariants reject active ownership" );
	return 0;
}

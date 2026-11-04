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

#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Resources/SIRResourceFileLoad.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#if defined( __linux__ )
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static std::atomic<int> s_nFailFstatOrdinal{ 0 };
static std::atomic<bool> s_bFailMappedRead{ false };
static std::atomic<bool> s_bFailMappedUnmap{ false };
static std::atomic<int> s_nFailPreadMode{ 0 };
static std::atomic<bool> s_bPreadEntered{ false };
static std::atomic<bool> s_bPreadRelease{ false };

extern "C" int __real_fstat( int nHandle, struct stat *pInfo );
extern "C" int __real_munmap( void *pBase, size_t nBytes );
extern "C" ssize_t __real_pread( int nHandle, void *pDestination, size_t nBytes, off_t nOffset );
extern "C" void *__real_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset );

extern "C" int __wrap_munmap( void *pBase, size_t nBytes )
{
	if ( s_bFailMappedUnmap.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	return __real_munmap( pBase, nBytes );
}

extern "C" ssize_t __wrap_pread( int nHandle, void *pDestination, size_t nBytes, off_t nOffset )
{
	const int nMode = s_nFailPreadMode.exchange( 0, std::memory_order_relaxed );
	if ( nMode == 1 ) {
		errno = EIO;
		return -1;
	}
	if ( nMode == 2 ) {
		return 0;
	}
	if ( nMode == 3 ) {
		s_bPreadEntered.store( true, std::memory_order_release );
		while ( !s_bPreadRelease.load( std::memory_order_acquire ) ) {
			std::this_thread::yield();
		}
	}
	return __real_pread( nHandle, pDestination, nBytes, nOffset );
}

extern "C" int __wrap_fstat( int nHandle, struct stat *pInfo )
{
	const int nOrdinal = s_nFailFstatOrdinal.load( std::memory_order_relaxed );
	if ( nOrdinal > 0 && s_nFailFstatOrdinal.fetch_sub( 1, std::memory_order_relaxed ) == 1 ) {
		errno = EIO;
		return -1;
	}
	return __real_fstat( nHandle, pInfo );
}

extern "C" void *__wrap_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset )
{
	if ( s_bFailMappedRead.exchange( false, std::memory_order_relaxed ) ) {
		errno = EACCES;
		return MAP_FAILED;
	}
	return __real_mmap( pAddress, nBytes, nProtection, nFlags, nHandle, nOffset );
}
#endif

#ifdef _WIN32
#include <malloc.h>
#endif

#define SIR_IO_CHECK( condition )                                                                                                          \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "GRDG I/O failure %s:%d: %s\n", __FILE__, __LINE__, #condition );                                             \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static constexpr const char *SIR_IO_PATH = "sir_grdg_io_test.bin";
static constexpr uint32_t SIR_IO_BYTES = 8192;
static constexpr uint32_t SIR_IO_OFFSET = 17;
static constexpr uint32_t SIR_IO_RANGE = 2048;

typedef struct sirIOTestAlloc_s
{
	std::atomic<int32_t> nOutstanding;
} sirIOTestAlloc_t;

static void *sirIOAlloc( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pMemory = nullptr;
#if defined( _WIN32 )
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory != nullptr ) {
		( ( sirIOTestAlloc_t * )pContext )->nOutstanding.fetch_add( 1 );
	}
	return pMemory;
}

static void sirIOFree( void *pContext, void *pMemory, uint64_t, uint32_t, uint32_t ) noexcept
{
	( ( sirIOTestAlloc_t * )pContext )->nOutstanding.fetch_sub( 1 );
#if defined( _WIN32 )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

static SIRResourceGraphConfig sirIOConfig( sirIOTestAlloc_t& rAlloc, uint32_t nStaging = 4 ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &rAlloc, sirIOAlloc, sirIOFree };
	config.m_nMetadataDomain = 0;
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = nStaging;
	config.m_nMaxProviderPins = 2;
	config.m_nMaxCommittedBytes = 4 * 1024 * 1024;
	return config;
}

static sirResourceFileLoadRequest_t sirIORequest( uint64_t nAsset, sirResourceFileReadMode_t eMode ) noexcept
{
	sirResourceFileLoadRequest_t request = {};
	request.keyResource = { nAsset, 91, 0 };
	request.keyContent = { UINT64_C( 0x1234 ), UINT64_C( 0xABCD ), 4, 0 };
	request.idChunk = { 0 };
	request.eKind = SIR_RESOURCE_KIND_BLOB;
	request.eMode = eMode;
	request.nSchemaVersion = 1;
	request.nFormatId = 4;
	request.nAlignment = 8;
	request.nStagingDomain = 6;
	request.nFileOffset = SIR_IO_OFFSET;
	request.nFileBytes = SIR_IO_RANGE;
	return request;
}

static void sirIOMemory( SIRMemoryManager& rMemory )
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	SIR_IO_CHECK( rMemory.init( config ) );
}

static void testConcurrentReads( const uint8_t *pData )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIR_IO_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 16, 2, 32 } ) );
		const sirResourceFileLoadRequest_t directRequest = sirIORequest( 1001, SIR_RESOURCE_FILE_READ_AT );
		const sirResourceFileLoadRequest_t mappedRequest = sirIORequest( 1002, SIR_RESOURCE_FILE_READ_MAPPED );
		SIRResourceHandle requested = {};
		SIR_IO_CHECK( graph.getWriter()->request( directRequest.keyResource, &requested ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceFileLoad direct( graph, scheduler );
		SIRResourceFileLoad mapped( graph, scheduler );
		SIR_IO_CHECK( direct.begin( SIR_IO_PATH, directRequest ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( mapped.begin( SIR_IO_PATH, mappedRequest ) == SIR_RESOURCE_RESULT_SUCCESS );
		sirTaskHandle_t taskA = {};
		sirTaskHandle_t taskB = {};
		SIR_IO_CHECK( direct.getTaskHandle( &taskA ) && mapped.getTaskHandle( &taskB ) );
		SIR_IO_CHECK( scheduler.waitTask( taskA ) && scheduler.waitTask( taskB ) );
		SIRResourceHandle first = {};
		SIRResourceHandle second = {};
		SIR_IO_CHECK( direct.poll( &first ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( mapped.poll( &second ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( first.m_nSlot == requested.m_nSlot && first.m_nGeneration == requested.m_nGeneration );
		SIR_IO_CHECK( !direct.isActive() && !mapped.isActive() );
		SIR_IO_CHECK( direct.getLastFileResult() == SIR_FILE_OK && mapped.getLastFileResult() == SIR_FILE_OK );
		SIR_IO_CHECK( graph.getStats().m_nUniqueChunks == 1 && graph.getStats().m_nPayloadBytes == SIR_IO_RANGE );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView firstView;
			SIRResourceView secondView;
			SIR_IO_CHECK( graph.resolve( scope, first, &firstView ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_IO_CHECK( graph.resolve( scope, second, &secondView ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceConstBytes bytesA = {};
			SIRResourceConstBytes bytesB = {};
			SIR_IO_CHECK( firstView.getChunk( { 0 }, &bytesA ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_IO_CHECK( secondView.getChunk( { 0 }, &bytesB ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_IO_CHECK( bytesA.m_nSize == SIR_IO_RANGE && bytesA.m_pData == bytesB.m_pData );
			SIR_IO_CHECK( memcmp( bytesA.m_pData, pData + SIR_IO_OFFSET, SIR_IO_RANGE ) == 0 );
		}
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

static void testCancellationAndStaleRevision( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		sirResourceFileLoadRequest_t request = sirIORequest( 2001, SIR_RESOURCE_FILE_READ_AT );

		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( load.poll( nullptr ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 0 );
		SIRResourceHandle ignored = {};
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( !load.isActive() );

		request.keyResource.m_nAssetId = 2002;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourcePublication newer = { request.keyResource, SIR_RESOURCE_KIND_BLOB, 1, 4, nullptr, 0, nullptr, 0 };
		SIRResourceHandle existing = {};
		SIR_IO_CHECK( graph.getWriter()->publish( newer, &existing ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		SIR_IO_CHECK( graph.getStats().m_nPayloadBytes == 0 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			SIR_IO_CHECK( graph.resolve( scope, existing, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_IO_CHECK( view.getRevision() == 1 );
		}
		std::atomic<uint32_t> result( 0 );
		std::thread wrongWriter( [&]() { result.store( load.begin( SIR_IO_PATH, request ) ); } );
		wrongWriter.join();
		SIR_IO_CHECK( result.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

static void testBoundsAndStagingReuse( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc, 1 ) );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad first( graph, scheduler );
		SIRResourceFileLoad second( graph, scheduler );
		sirResourceFileLoadRequest_t request = sirIORequest( 3001, SIR_RESOURCE_FILE_READ_AT );
		SIR_IO_CHECK( first.begin( "sir_grdg_io_missing_file.bin", request ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( first.getLastFileResult() == SIR_FILE_NOT_FOUND );
		request.nFileOffset = SIR_IO_BYTES;
		SIR_IO_CHECK( first.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_IO_CHECK( first.getLastFileResult() == SIR_FILE_OUT_OF_RANGE );
		request.nFileOffset = SIR_IO_OFFSET;
		request.nFileBytes = 0;
		SIR_IO_CHECK( first.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nFileBytes = SIR_IO_RANGE;
		SIR_IO_CHECK( first.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		request.keyResource.m_nAssetId = 3002;
		SIR_IO_CHECK( second.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		SIR_IO_CHECK( !second.isActive() );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIRResourceHandle firstHandle = {};
		SIR_IO_CHECK( first.poll( &firstHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( second.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIRResourceHandle secondHandle = {};
		SIR_IO_CHECK( second.poll( &secondHandle ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( graph.getStats().m_nUniqueChunks == 1 );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

static void testExternalTaskReleaseRecovery( void )
{
	sirIOTestAlloc_t allocator = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( allocator ) );
		SIR_IO_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		const int32_t nBefore = allocator.nOutstanding.load();
		const sirResourceFileLoadRequest_t request = sirIORequest( 6011, SIR_RESOURCE_FILE_READ_AT );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		sirTaskHandle_t task = {};
		SIR_IO_CHECK( load.getTaskHandle( &task ) );
		SIR_IO_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIR_IO_CHECK( scheduler.releaseTask( task ) );
		SIRResourceHandle ignored = {};
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( !load.isActive() );
		SIR_IO_CHECK( allocator.nOutstanding.load() == nBefore );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( allocator.nOutstanding.load() == 0 );
	memory.shutdown();
}

static void testDiscardAfterTerminalTransfer( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIR_IO_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		const int32_t nBaseline = alloc.nOutstanding.load();
		SIRResourceHandle resource = {};
		sirResourceFileLoadRequest_t request = sirIORequest( 6021, SIR_RESOURCE_FILE_READ_AT );

		// A completed transfer can still be explicitly discarded before publication.
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( !load.isActive() && graph.getStats().m_nLiveResources == 0 );
		SIR_IO_CHECK( alloc.nOutstanding.load() == nBaseline );

#if defined( __linux__ )
		// Discarding an already-failed native I/O task must also release staging.
		request.keyResource.m_nAssetId = 6022;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		s_nFailPreadMode.store( 1, std::memory_order_relaxed );
		SIR_IO_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_IO_ERROR );
		SIR_IO_CHECK( alloc.nOutstanding.load() == nBaseline );
#endif

		// The same transfer object must remain reusable after both terminal paths.
		request.keyResource.m_nAssetId = 6023;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( graph.getStats().m_nLiveResources == 1 );
		SIR_IO_CHECK( graph.getWriter()->retire( resource ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( graph.getWriter()->collectRetired( 4 ) == 1 );
		SIR_IO_CHECK( alloc.nOutstanding.load() == nBaseline );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

static sirTaskResult_t sirIOIdleTask( void *, sirTaskHandle_t ) noexcept
{
	return SIR_TASK_RESULT_SUCCESS;
}

static void testRejectedRequestsAndTaskAdmission( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		const sirResourceFileLoadRequest_t valid = sirIORequest( 4001, SIR_RESOURCE_FILE_READ_AT );
		sirResourceFileLoadRequest_t request = valid;
		SIRResourceHandle resource = {};
		sirTaskHandle_t task = {};
		SIR_IO_CHECK( !load.isActive() && !load.getTaskHandle( &task ) );
		SIR_IO_CHECK( !load.getTaskHandle( nullptr ) );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_IO_CHECK( load.begin( nullptr, valid ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_IO_CHECK( load.begin( "", valid ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.keyResource.m_nTypeId = 0;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.eKind = ( SIRResourceKind )255;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.eMode = ( sirResourceFileReadMode_t )255;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.nSchemaVersion = 0;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.nAlignment = 0;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.nAlignment = 3;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.nDependencyCount = 1;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		request.nFileOffset = 8191;
		request.nFileBytes = 2;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request = valid;
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, valid ) == SIR_RESOURCE_RESULT_BUSY );
		SIR_IO_CHECK( !load.getTaskHandle( nullptr ) && load.getTaskHandle( &task ) );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_BUSY );
		std::atomic<uint32_t> nWrongWriter( 0 );
		std::thread wrongWriter( [&]() {
			if ( load.cancel() == SIR_RESOURCE_RESULT_WRITER_VIOLATION && load.poll( &resource ) == SIR_RESOURCE_RESULT_WRITER_VIOLATION &&
			     !load.getTaskHandle( &task ) ) {
				nWrongWriter.store( 1, std::memory_order_relaxed );
			}
		} );
		wrongWriter.join();
		SIR_IO_CHECK( nWrongWriter.load( std::memory_order_relaxed ) == 1 );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 0 );
		SIR_IO_CHECK( load.poll( &resource ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( !load.isActive() && load.cancel() == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		sirTaskHandle_t occupied[8] = {};
		for ( uint32_t i = 0; i < 8; ++i ) {
			SIR_IO_CHECK( scheduler.submit( sirIOIdleTask, nullptr, SIR_TASK_PRIORITY_BACKGROUND, SIR_TASK_EXEC_CPU, &occupied[i] ) );
		}
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, valid ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		SIR_IO_CHECK( !load.isActive() );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 8 );
		for ( uint32_t i = 0; i < 8; ++i ) {
			SIR_IO_CHECK( scheduler.releaseTask( occupied[i] ) );
		}
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

#if defined( __linux__ )
static void testCancellationOfRunningNativeRead( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIR_IO_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 1, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		const int32_t nBaseline = alloc.nOutstanding.load();
		const sirResourceFileLoadRequest_t request = sirIORequest( 6031, SIR_RESOURCE_FILE_READ_AT );
		s_bPreadEntered.store( false, std::memory_order_release );
		s_bPreadRelease.store( false, std::memory_order_release );
		s_nFailPreadMode.store( 3, std::memory_order_release );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		bool bEntered = false;
		for ( uint32_t nAttempt = 0; nAttempt < 2000000; ++nAttempt ) {
			if ( s_bPreadEntered.load( std::memory_order_acquire ) ) {
				bEntered = true;
				break;
			}
			std::this_thread::yield();
		}
		SIR_IO_CHECK( bEntered );
		sirTaskHandle_t task = {};
		SIR_IO_CHECK( load.getTaskHandle( &task ) );
		SIR_IO_CHECK( scheduler.getState( task ) == SIR_TASK_RUNNING );
		SIRResourceHandle ignored = {};
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_BUSY );
		SIR_IO_CHECK( load.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_IO_CHECK( scheduler.isCancellationRequested( task ) );
		s_bPreadRelease.store( true, std::memory_order_release );
		SIR_IO_CHECK( scheduler.waitTask( task ) );
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( !load.isActive() && graph.getStats().m_nLiveResources == 0 );
		SIR_IO_CHECK( alloc.nOutstanding.load() == nBaseline );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}

static void testInjectedResourceTransferFailures( void )
{
	sirIOTestAlloc_t alloc = {};
	SIRMemoryManager memory;
	sirIOMemory( memory );
	{
		SIRResourceGraph graph( sirIOConfig( alloc ) );
		SIR_IO_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_IO_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		sirResourceFileLoadRequest_t request = sirIORequest( 5001, SIR_RESOURCE_FILE_READ_AT );
		s_nFailFstatOrdinal.store( 2, std::memory_order_relaxed );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_IO_ERROR && !load.isActive() );
		SIR_IO_CHECK( s_nFailFstatOrdinal.load( std::memory_order_relaxed ) == 0 );
		request = sirIORequest( 5002, SIR_RESOURCE_FILE_READ_MAPPED );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		s_bFailMappedRead.store( true, std::memory_order_relaxed );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIRResourceHandle ignored = {};
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_ACCESS_DENIED );
		SIR_IO_CHECK( !load.isActive() );
		request = sirIORequest( 5003, SIR_RESOURCE_FILE_READ_AT );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		s_nFailPreadMode.store( 1, std::memory_order_relaxed );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_IO_ERROR );
		request = sirIORequest( 5004, SIR_RESOURCE_FILE_READ_AT );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		s_nFailPreadMode.store( 2, std::memory_order_relaxed );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_END_OF_FILE );
		SIR_IO_CHECK( !load.isActive() );
		request = sirIORequest( 5005, SIR_RESOURCE_FILE_READ_MAPPED );
		SIR_IO_CHECK( load.begin( SIR_IO_PATH, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		s_bFailMappedUnmap.store( true, std::memory_order_relaxed );
		SIR_IO_CHECK( scheduler.pumpCPU( 8 ) == 1 );
		SIR_IO_CHECK( load.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_IO_CHECK( load.getLastFileResult() == SIR_FILE_IO_ERROR );
		SIR_IO_CHECK( !load.isActive() );
		SIR_IO_CHECK( scheduler.shutdown() );
	}
	SIR_IO_CHECK( alloc.nOutstanding.load() == 0 );
	memory.shutdown();
}
#endif

int main( void )
{
	uint8_t bytes[SIR_IO_BYTES] = {};
	for ( uint32_t i = 0; i < SIR_IO_BYTES; ++i ) {
		bytes[i] = ( uint8_t )( ( i * 37 + 19 ) & 255 );
	}
	SIRPlatformFile::remove( SIR_IO_PATH );
	SIRPlatformFile file;
	SIR_IO_CHECK( file.open( SIR_IO_PATH, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	uint64_t nTransferred = 0;
	SIR_IO_CHECK( file.writeAt( 0, bytes, SIR_IO_BYTES, &nTransferred ) == SIR_FILE_OK && nTransferred == SIR_IO_BYTES );
	SIR_IO_CHECK( file.close() == SIR_FILE_OK );
	testConcurrentReads( bytes );
	testCancellationAndStaleRevision();
	testBoundsAndStagingReuse();
	testRejectedRequestsAndTaskAdmission();
	testExternalTaskReleaseRecovery();
	testDiscardAfterTerminalTransfer();
#if defined( __linux__ )
	testInjectedResourceTransferFailures();
	testCancellationOfRunningNativeRead();
#endif
	SIR_IO_CHECK( SIRPlatformFile::remove( SIR_IO_PATH ) == SIR_FILE_OK );
	printf( "PASS: native file reads, mapped GRDG staging, Core Jobs, deduplication, revision and cancellation\n" );
	return 0;
}

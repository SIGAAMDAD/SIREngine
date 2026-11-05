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
#include <memory>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

#define SIR_INTEGRATION_CHECK( condition )                                                                                                 \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "Infrastructure regression %s:%d: %s\n", __FILE__, __LINE__, #condition );                                    \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static constexpr const char *SIR_INTEGRATION_FILE = "sir_infrastructure_stress_fixture.bin";
static constexpr uint64_t SIR_INTEGRATION_FILE_SIZE = 2 * 1024 * 1024;

typedef struct sirInfrastructureAllocStats_s
{
	std::atomic<int64_t> nLiveAllocations;
	std::atomic<int64_t> nLiveBytes;
	std::atomic<uint64_t> nTotalAllocations;
} sirInfrastructureAllocStats_t;

static void *sirInfrastructureAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pResult = nullptr;
#if defined( _WIN32 )
	pResult = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pResult, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pResult != nullptr ) {
		sirInfrastructureAllocStats_t *pStats = ( sirInfrastructureAllocStats_t * )pContext;
		pStats->nLiveAllocations.fetch_add( 1, std::memory_order_relaxed );
		pStats->nLiveBytes.fetch_add( ( int64_t )nBytes, std::memory_order_relaxed );
		pStats->nTotalAllocations.fetch_add( 1, std::memory_order_relaxed );
	}
	return pResult;
}

static void sirInfrastructureRelease( void *pContext, void *pData, uint64_t nBytes, uint32_t, uint32_t ) noexcept
{
	sirInfrastructureAllocStats_t *pStats = ( sirInfrastructureAllocStats_t * )pContext;
	pStats->nLiveAllocations.fetch_sub( 1, std::memory_order_relaxed );
	pStats->nLiveBytes.fetch_sub( ( int64_t )nBytes, std::memory_order_relaxed );
#if defined( _WIN32 )
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static SIRResourceGraphConfig sirInfrastructureConfig( sirInfrastructureAllocStats_t& rStats, uint32_t nResources,
                                                       uint32_t nStaging ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &rStats, sirInfrastructureAllocate, sirInfrastructureRelease };
	config.m_nMetadataDomain = 0;
	config.m_nMaxResources = nResources;
	config.m_nMaxStaging = nStaging;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 24 * 1024 * 1024;
	config.nMaxReaders = 16;
	return config;
}

static void sirInfrastructureMemory( SIRMemoryManager& rMemory )
{
	SIR_INTEGRATION_CHECK( rMemory.init( { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 } ) );
}

static sirResourceFileLoadRequest_t sirInfrastructureRequest( uint64_t nAsset, sirResourceFileReadMode_t eMode, uint64_t nOffset,
                                                              uint64_t nBytes, uint64_t nExpectedRevision = 0 ) noexcept
{
	sirResourceFileLoadRequest_t request = {};
	request.keyResource = { nAsset, 71, 0 };
	request.keyContent = { nAsset ^ UINT64_C( 0x9e3779b97f4a7c15 ), nOffset ^ nBytes, 71, 0 };
	request.idChunk = { 0 };
	request.eKind = SIR_RESOURCE_KIND_BLOB;
	request.eMode = eMode;
	request.nSchemaVersion = 1;
	request.nFormatId = 71;
	request.nAlignment = 16;
	request.nStagingDomain = 0;
	request.nFileOffset = nOffset;
	request.nFileBytes = nBytes;
	request.nExpectedRevision = nExpectedRevision;
	return request;
}

static void sirInfrastructureMakeFile( const std::vector<uint8_t>& rBytes )
{
	SIRPlatformFile::remove( SIR_INTEGRATION_FILE );
	SIRPlatformFile file;
	SIR_INTEGRATION_CHECK( file.open( SIR_INTEGRATION_FILE, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) ==
	                       SIR_FILE_OK );
	uint64_t nWritten = 0;
	SIR_INTEGRATION_CHECK( file.writeAt( 0, rBytes.data(), rBytes.size(), &nWritten ) == SIR_FILE_OK );
	SIR_INTEGRATION_CHECK( nWritten == rBytes.size() );
	SIR_INTEGRATION_CHECK( file.close() == SIR_FILE_OK );
}

static void sirInfrastructureVerify( SIRResourceGraph& rGraph, SIRResourceHandle hResource, const std::vector<uint8_t>& rBytes,
                                     uint64_t nOffset, uint64_t nCount, uint64_t nRevision )
{
	SIRResourceReadScope scope( rGraph );
	SIR_INTEGRATION_CHECK( scope.isActive() );
	SIRResourceView view;
	SIR_INTEGRATION_CHECK( rGraph.resolve( scope, hResource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_INTEGRATION_CHECK( view.getRevision() == nRevision );
	SIRResourceConstBytes bytes = {};
	SIR_INTEGRATION_CHECK( view.getChunk( { 0 }, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_INTEGRATION_CHECK( bytes.m_nSize == nCount );
	SIR_INTEGRATION_CHECK( memcmp( bytes.m_pData, rBytes.data() + nOffset, ( size_t )nCount ) == 0 );
}

static void sirInfrastructureRunMatrix( const std::vector<uint8_t>& rBytes )
{
	sirInfrastructureAllocStats_t alloc = {};
	SIRMemoryManager memory;
	sirInfrastructureMemory( memory );
	{
		SIRResourceGraph graph( sirInfrastructureConfig( alloc, 128, 4 ) );
		SIR_INTEGRATION_CHECK( graph.isReady() );
		SIRTaskScheduler scheduler;
		SIR_INTEGRATION_CHECK( scheduler.init( &memory, { 32, 0, 64 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		const uint64_t offsets[] = { 0, 1, 4095, 4096, 65535 };
		const uint64_t lengths[] = { 1, 17, 4093, 65536, 262144 };
		uint64_t nAsset = 1000;
		for ( uint32_t eMode = 0; eMode < 2; ++eMode ) {
			for ( uint64_t nOffset : offsets ) {
				for ( uint64_t nCount : lengths ) {
					const sirResourceFileReadMode_t mode = eMode == 0 ? SIR_RESOURCE_FILE_READ_AT : SIR_RESOURCE_FILE_READ_MAPPED;
					sirResourceFileLoadRequest_t request = sirInfrastructureRequest( nAsset++, mode, nOffset, nCount );
					SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_SUCCESS );
					SIRResourceHandle pending = {};
					SIR_INTEGRATION_CHECK( load.poll( &pending ) == SIR_RESOURCE_RESULT_BUSY );
					SIR_INTEGRATION_CHECK( scheduler.pumpCPU( 1 ) == 1 );
					SIRResourceHandle published = {};
					SIR_INTEGRATION_CHECK( load.poll( &published ) == SIR_RESOURCE_RESULT_SUCCESS );
					sirInfrastructureVerify( graph, published, rBytes, nOffset, nCount, 1 );
					SIR_INTEGRATION_CHECK( !load.isActive() );
					SIR_INTEGRATION_CHECK( load.getLastFileResult() == SIR_FILE_OK );
				}
			}
		}
		SIR_INTEGRATION_CHECK( graph.getStats().m_nLiveResources == 50 );
		SIR_INTEGRATION_CHECK( scheduler.getActiveCount() == 0 );
		SIR_INTEGRATION_CHECK( scheduler.shutdown() );
	}
	SIR_INTEGRATION_CHECK( alloc.nLiveAllocations.load() == 0 );
	SIR_INTEGRATION_CHECK( alloc.nLiveBytes.load() == 0 );
	memory.shutdown();
}

static void sirInfrastructureRunBackpressureAndCancellation( void )
{
	sirInfrastructureAllocStats_t alloc = {};
	SIRMemoryManager memory;
	sirInfrastructureMemory( memory );
	{
		SIRResourceGraph graph( sirInfrastructureConfig( alloc, 16, 2 ) );
		SIRTaskScheduler scheduler;
		SIR_INTEGRATION_CHECK( scheduler.init( &memory, { 1, 0, 2 } ) );
		SIRResourceFileLoad first( graph, scheduler );
		SIRResourceFileLoad second( graph, scheduler );
		const sirResourceFileLoadRequest_t a = sirInfrastructureRequest( 2001, SIR_RESOURCE_FILE_READ_AT, 17, 16384 );
		const sirResourceFileLoadRequest_t b = sirInfrastructureRequest( 2002, SIR_RESOURCE_FILE_READ_MAPPED, 97, 16384 );
		SIR_INTEGRATION_CHECK( first.begin( SIR_INTEGRATION_FILE, a ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_INTEGRATION_CHECK( first.begin( SIR_INTEGRATION_FILE, a ) == SIR_RESOURCE_RESULT_BUSY );
		SIR_INTEGRATION_CHECK( second.begin( SIR_INTEGRATION_FILE, b ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		SIR_INTEGRATION_CHECK( !second.isActive() );
		SIR_INTEGRATION_CHECK( first.cancel() == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_INTEGRATION_CHECK( scheduler.pumpCPU( 4 ) == 0 );
		SIRResourceHandle ignored = {};
		SIR_INTEGRATION_CHECK( first.poll( &ignored ) == SIR_RESOURCE_RESULT_FAILED );
		SIR_INTEGRATION_CHECK( scheduler.getActiveCount() == 0 );
		SIR_INTEGRATION_CHECK( second.begin( SIR_INTEGRATION_FILE, b ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_INTEGRATION_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIR_INTEGRATION_CHECK( second.poll( &ignored ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_INTEGRATION_CHECK( scheduler.shutdown() );
	}
	SIR_INTEGRATION_CHECK( alloc.nLiveAllocations.load() == 0 );
	memory.shutdown();
}

static void sirInfrastructureRunConcurrentWorkers( const std::vector<uint8_t>& rBytes )
{
	sirInfrastructureAllocStats_t alloc = {};
	SIRMemoryManager memory;
	sirInfrastructureMemory( memory );
	{
		SIRResourceGraph graph( sirInfrastructureConfig( alloc, 32, 12 ) );
		SIRTaskScheduler scheduler;
		SIR_INTEGRATION_CHECK( scheduler.init( &memory, { 32, 4, 64 } ) );
		std::vector<std::unique_ptr<SIRResourceFileLoad>> jobs;
		std::vector<sirTaskHandle_t> handles( 12 );
		for ( uint32_t i = 0; i < handles.size(); ++i ) {
			jobs.emplace_back( new SIRResourceFileLoad( graph, scheduler ) );
			const uint64_t nOffset = i * 131;
			const uint64_t nCount = 4096 + i * 173;
			auto mode = i & 1 ? SIR_RESOURCE_FILE_READ_MAPPED : SIR_RESOURCE_FILE_READ_AT;
			auto request = sirInfrastructureRequest( 3000 + i, mode, nOffset, nCount );
			SIR_INTEGRATION_CHECK( jobs.back()->begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_INTEGRATION_CHECK( jobs.back()->getTaskHandle( &handles[i] ) );
		}
		for ( uint32_t i = 0; i < handles.size(); ++i ) {
			SIR_INTEGRATION_CHECK( scheduler.waitTask( handles[i] ) );
			SIRResourceHandle resource = {};
			SIR_INTEGRATION_CHECK( jobs[i]->poll( &resource ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirInfrastructureVerify( graph, resource, rBytes, i * 131, 4096 + i * 173, 1 );
		}
		SIR_INTEGRATION_CHECK( scheduler.getActiveCount() == 0 );
		SIR_INTEGRATION_CHECK( scheduler.shutdown() );
	}
	SIR_INTEGRATION_CHECK( alloc.nLiveAllocations.load() == 0 );
	memory.shutdown();
}

static void sirInfrastructureRunRevisionChurn( const std::vector<uint8_t>& rBytes )
{
	sirInfrastructureAllocStats_t alloc = {};
	SIRMemoryManager memory;
	sirInfrastructureMemory( memory );
	{
		SIRResourceGraph graph( sirInfrastructureConfig( alloc, 8, 1 ) );
		SIRTaskScheduler scheduler;
		SIR_INTEGRATION_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		for ( uint64_t i = 0; i < 256; ++i ) {
			auto request = sirInfrastructureRequest( 4001, i & 1 ? SIR_RESOURCE_FILE_READ_MAPPED : SIR_RESOURCE_FILE_READ_AT,
			                                         ( i * 67 ) % 8192, 1024, i );
			request.keyContent.m_nHashLo += i;
			request.keyContent.m_nHashHi += i;
			SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIR_INTEGRATION_CHECK( scheduler.pumpCPU( 1 ) == 1 );
			SIRResourceHandle published = {};
			SIR_INTEGRATION_CHECK( load.poll( &published ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirInfrastructureVerify( graph, published, rBytes, request.nFileOffset, 1024, i + 1 );
			graph.getWriter()->collectRetired( UINT32_MAX );
			SIR_INTEGRATION_CHECK( graph.getStats().m_nLiveResources == 1 );
		}
		SIR_INTEGRATION_CHECK( graph.getStats().m_nCommittedBytes < 4 * 1024 * 1024 );
		SIR_INTEGRATION_CHECK( scheduler.getActiveCount() == 0 );
		SIR_INTEGRATION_CHECK( scheduler.shutdown() );
	}
	SIR_INTEGRATION_CHECK( alloc.nLiveAllocations.load() == 0 );
	memory.shutdown();
}

static void sirInfrastructureRunRejections( void )
{
	sirInfrastructureAllocStats_t alloc = {};
	SIRMemoryManager memory;
	sirInfrastructureMemory( memory );
	{
		SIRResourceGraph graph( sirInfrastructureConfig( alloc, 8, 1 ) );
		SIRTaskScheduler scheduler;
		SIR_INTEGRATION_CHECK( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRResourceFileLoad load( graph, scheduler );
		auto request = sirInfrastructureRequest( 6001, SIR_RESOURCE_FILE_READ_AT, 0, 4096 );
		SIR_INTEGRATION_CHECK( load.begin( nullptr, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		SIR_INTEGRATION_CHECK( load.begin( "", request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nAlignment = 3;
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nAlignment = 16;
		request.nFileBytes = 0;
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nFileBytes = 4096;
		request.nFileOffset = UINT64_MAX;
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nFileOffset = 0;
		request.eMode = ( sirResourceFileReadMode_t )255;
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.eMode = SIR_RESOURCE_FILE_READ_AT;
		request.nDependencyCount = 1;
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		request.nDependencyCount = 0;
		std::atomic<uint32_t> nOther( 0 );
		std::thread wrongWriter( [&]() { nOther.store( ( uint32_t )load.begin( SIR_INTEGRATION_FILE, request ) ); } );
		wrongWriter.join();
		SIR_INTEGRATION_CHECK( nOther.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
		SIR_INTEGRATION_CHECK( !load.getTaskHandle( nullptr ) );
		SIR_INTEGRATION_CHECK( load.begin( SIR_INTEGRATION_FILE, request ) == SIR_RESOURCE_RESULT_SUCCESS );
		std::thread wrongPoller( [&]() {
			SIRResourceHandle ignored = {};
			nOther.store( ( uint32_t )load.poll( &ignored ) );
		} );
		wrongPoller.join();
		SIR_INTEGRATION_CHECK( nOther.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
		SIR_INTEGRATION_CHECK( scheduler.pumpCPU( 1 ) == 1 );
		SIRResourceHandle published = {};
		SIR_INTEGRATION_CHECK( load.poll( &published ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIR_INTEGRATION_CHECK( scheduler.shutdown() );
	}
	SIR_INTEGRATION_CHECK( alloc.nLiveAllocations.load() == 0 );
	memory.shutdown();
}

int main( void )
{
	std::vector<uint8_t> bytes( ( size_t )SIR_INTEGRATION_FILE_SIZE );
	for ( uint64_t i = 0; i < SIR_INTEGRATION_FILE_SIZE; ++i ) {
		bytes[( size_t )i] = ( uint8_t )( ( i * 131 + ( i >> 7 ) + 17 ) & 255 );
	}
	sirInfrastructureMakeFile( bytes );
	sirInfrastructureRunMatrix( bytes );
	sirInfrastructureRunBackpressureAndCancellation();
	sirInfrastructureRunConcurrentWorkers( bytes );
	sirInfrastructureRunRevisionChurn( bytes );
	sirInfrastructureRunRejections();
	SIR_INTEGRATION_CHECK( SIRPlatformFile::remove( SIR_INTEGRATION_FILE ) == SIR_FILE_OK );
	printf( "PASS: GRDG/native I/O matrix, scheduler backpressure, multithreaded reads, revision churn and failures\n" );
	return 0;
}

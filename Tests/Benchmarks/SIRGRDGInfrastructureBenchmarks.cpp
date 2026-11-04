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
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Core/Platform/SIRPlatformFileMapping.h>
#include <Core/Platform/SIRVirtualMemory.h>
#include <Resources/SIRResourceFileLoad.h>

#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

static constexpr const char *SIR_GRDG_BENCHMARK_FILE = "sir_grdg_benchmark_fixture.bin";
static constexpr uint64_t SIR_GRDG_BENCHMARK_FILE_BYTES = 2 * 1024 * 1024;

typedef struct sirGRDGBenchmarkAllocator_s
{
	std::atomic<int64_t> nOutstanding;
	std::atomic<uint64_t> nAllocations;
} sirGRDGBenchmarkAllocator_t;

static void *sirGRDGBenchmarkAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	if ( nBytes == 0 || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pData = nullptr;
#if defined( _WIN32 )
	pData = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pData, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pData != nullptr ) {
		sirGRDGBenchmarkAllocator_t *pStats = ( sirGRDGBenchmarkAllocator_t * )pContext;
		pStats->nOutstanding.fetch_add( 1, std::memory_order_relaxed );
		pStats->nAllocations.fetch_add( 1, std::memory_order_relaxed );
	}
	return pData;
}

static void sirGRDGBenchmarkFree( void *pContext, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
	( ( sirGRDGBenchmarkAllocator_t * )pContext )->nOutstanding.fetch_sub( 1, std::memory_order_relaxed );
#if defined( _WIN32 )
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static SIRResourceGraphConfig sirGRDGBenchmarkConfig( sirGRDGBenchmarkAllocator_t &rAllocator, uint32_t nResources ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &rAllocator, sirGRDGBenchmarkAllocate, sirGRDGBenchmarkFree };
	config.m_nMetadataDomain = 0;
	config.m_nMaxResources = nResources;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 32ULL * 1024 * 1024;
	config.nMaxReaders = 16;
	return config;
}

static bool sirGRDGBenchmarkMemory( SIRMemoryManager &rMemory ) noexcept
{
	return rMemory.init( { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 } );
}

class SIRGRDGBenchmarkFixture
{
public:
	std::vector<uint8_t> m_Bytes;
	SIRPlatformFile m_File;

	bool init( void )
	{
		m_Bytes.resize( ( size_t )SIR_GRDG_BENCHMARK_FILE_BYTES );
		for ( uint64_t i = 0; i < SIR_GRDG_BENCHMARK_FILE_BYTES; ++i ) {
			m_Bytes[( size_t )i] = ( uint8_t )( ( i * 83 + ( i >> 8 ) + 41 ) & 255 );
		}
		SIRPlatformFile::remove( SIR_GRDG_BENCHMARK_FILE );
		if ( m_File.open( SIR_GRDG_BENCHMARK_FILE, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) != SIR_FILE_OK ) {
			return false;
		}
		uint64_t nWritten = 0;
		if ( m_File.writeAt( 0, m_Bytes.data(), SIR_GRDG_BENCHMARK_FILE_BYTES, &nWritten ) != SIR_FILE_OK ||
		     nWritten != SIR_GRDG_BENCHMARK_FILE_BYTES || m_File.close() != SIR_FILE_OK ) {
			return false;
		}
		return m_File.open( SIR_GRDG_BENCHMARK_FILE, SIR_FILE_OPEN_READ ) == SIR_FILE_OK;
	}

	~SIRGRDGBenchmarkFixture( void )
	{
		m_File.close();
		SIRPlatformFile::remove( SIR_GRDG_BENCHMARK_FILE );
	}
};

static void SIRBenchmarkGRDGNativeReadAt( benchmark::State &state )
{
	SIRGRDGBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "Cannot create native I/O benchmark fixture" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	std::vector<uint8_t> output( ( size_t )nBytes );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		uint64_t nRead = 0;
		if ( fixture.m_File.readAt( 17, output.data(), nBytes, &nRead ) != SIR_FILE_OK || nRead != nBytes ) {
			state.SkipWithError( "SIRPlatformFile::readAt failed" );
			bFailed = true;
			break;
		}
		benchmark::DoNotOptimize( output.data() );
	}
	if ( !bFailed && memcmp( output.data(), fixture.m_Bytes.data() + 17, ( size_t )nBytes ) != 0 ) {
		state.SkipWithError( "Native read content mismatch" );
	}
	state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
}

static void SIRBenchmarkGRDGMappedCopy( benchmark::State &state )
{
	SIRGRDGBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "Cannot create mapped I/O benchmark fixture" );
		return;
	}
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	std::vector<uint8_t> output( ( size_t )nBytes );
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		SIRPlatformFileMapping mapping;
		if ( mapping.map( fixture.m_File, 17, nBytes, SIR_FILE_MAP_READ_ONLY ) != SIR_FILE_OK ) {
			state.SkipWithError( "Native file mapping failed" );
			bFailed = true;
			break;
		}
		memcpy( output.data(), mapping.getData(), ( size_t )nBytes );
		benchmark::DoNotOptimize( output.data() );
		if ( mapping.unmap() != SIR_FILE_OK ) {
			state.SkipWithError( "Native file unmapping failed" );
			bFailed = true;
			break;
		}
	}
	if ( !bFailed && memcmp( output.data(), fixture.m_Bytes.data() + 17, ( size_t )nBytes ) != 0 ) {
		state.SkipWithError( "Mapped content mismatch" );
	}
	state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
}

static void SIRBenchmarkGRDGLookup( benchmark::State &state )
{
	const uint32_t nResources = ( uint32_t )state.range( 0 );
	sirGRDGBenchmarkAllocator_t allocator = {};
	SIRResourceGraph graph( sirGRDGBenchmarkConfig( allocator, nResources + 8 ) );
	if ( !graph.isReady() ) {
		state.SkipWithError( "GRDG lookup graph init failed" );
		return;
	}
	for ( uint32_t i = 0; i < nResources; ++i ) {
		SIRResourcePublication publication = { { i + 1, 75, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 7, nullptr, 0, nullptr, 0 };
		SIRResourceHandle handle = {};
		if ( graph.getWriter()->publish( publication, &handle ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			state.SkipWithError( "GRDG lookup setup failed" );
			return;
		}
	}
	uint64_t nLookups = 0;
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		const SIRResourceKey key = { ( nLookups % nResources ) + 1, 75, 0 };
		SIRResourceReadScope scope( graph );
		SIRResourceHandle handle = {};
		SIRResourceView view;
		if ( !scope.isActive() || graph.find( scope, key, &handle ) != SIR_RESOURCE_RESULT_SUCCESS ||
		     graph.resolve( scope, handle, &view ) != SIR_RESOURCE_RESULT_SUCCESS || view.getRevision() != 1 ) {
			state.SkipWithError( "GRDG lookup/resolve failed" );
			bFailed = true;
			break;
		}
		benchmark::DoNotOptimize( handle );
		++nLookups;
	}
	if ( !bFailed ) {
		state.SetItemsProcessed( ( int64_t )nLookups );
	}
	state.counters["active_records"] = ( double )graph.getStats().m_nLiveResources;
}

static void SIRBenchmarkGRDGPublishReplace( benchmark::State &state )
{
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	sirGRDGBenchmarkAllocator_t allocator = {};
	SIRResourceGraph graph( sirGRDGBenchmarkConfig( allocator, 8 ) );
	if ( !graph.isReady() ) {
		state.SkipWithError( "GRDG publish graph init failed" );
		return;
	}
	const SIRResourceKey key = { 8001, 76, 0 };
	uint64_t nRevision = 0;
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes bytes = {};
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		if ( pWriter->allocateStaging( nBytes, 16, 0, &staging, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			state.SkipWithError( "GRDG staging budget exhausted" );
			bFailed = true;
			break;
		}
		memset( bytes.m_pData, ( int )( nRevision & 255 ), ( size_t )nBytes );
		const SIRResourceContentKey content = { nRevision + 1, UINT64_C( 0xa17c4f ), 7, 0 };
		const SIRResourceChunkPublication chunk = { { 0 }, content, staging, false };
		const SIRResourcePublication publication = { key, SIR_RESOURCE_KIND_BLOB, 1, 7, &chunk, 1, nullptr, 0 };
		SIRResourceHandle handle = {};
		if ( pWriter->publishExpected( publication, nRevision, &handle ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			pWriter->cancelStaging( staging );
			state.SkipWithError( "GRDG transactional publication failed" );
			bFailed = true;
			break;
		}
		++nRevision;
		pWriter->collectRetired( UINT32_MAX );
		benchmark::DoNotOptimize( handle );
	}
	if ( !bFailed ) {
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
	state.counters["allocator_calls"] = ( double )allocator.nAllocations.load();
	state.counters["committed_bytes"] = ( double )graph.getStats().m_nCommittedBytes;
}

static void SIRBenchmarkGRDGFileIngest( benchmark::State &state )
{
	const uint64_t nBytes = ( uint64_t )state.range( 0 );
	const sirResourceFileReadMode_t eMode = state.range( 1 ) == 0 ? SIR_RESOURCE_FILE_READ_AT : SIR_RESOURCE_FILE_READ_MAPPED;
	SIRGRDGBenchmarkFixture fixture;
	if ( !fixture.init() ) {
		state.SkipWithError( "Cannot create GRDG ingest fixture" );
		return;
	}
	sirGRDGBenchmarkAllocator_t allocator = {};
	SIRMemoryManager memory;
	if ( !sirGRDGBenchmarkMemory( memory ) ) {
		state.SkipWithError( "Cannot initialize GRDG benchmark memory" );
		return;
	}
	{
		SIRResourceGraph graph( sirGRDGBenchmarkConfig( allocator, 8 ) );
		SIRTaskScheduler scheduler;
		if ( !graph.isReady() || !scheduler.init( &memory, { 8, 0, 16 } ) ) {
			state.SkipWithError( "GRDG ingest scheduler/graph init failed" );
			return;
		}
		SIRResourceFileLoad loader( graph, scheduler );
		sirResourceFileLoadRequest_t request = {};
		request.keyResource = { 9001, 77, 0 };
		request.keyContent = { UINT64_C( 0x1234 ), UINT64_C( 0xabcd ), 7, 0 };
		request.idChunk = { 0 };
		request.eKind = SIR_RESOURCE_KIND_BLOB;
		request.eMode = eMode;
		request.nSchemaVersion = 1;
		request.nFormatId = 7;
		request.nAlignment = 16;
		request.nStagingDomain = 0;
		request.nFileOffset = 17;
		request.nFileBytes = nBytes;
		uint64_t nRevision = 0;
		bool bFailed = false;
		for ( auto _ : state ) {
			( void )_;
			request.nExpectedRevision = nRevision;
			if ( loader.begin( SIR_GRDG_BENCHMARK_FILE, request ) != SIR_RESOURCE_RESULT_SUCCESS || scheduler.pumpCPU( 1 ) != 1 ) {
				state.SkipWithError( "GRDG ingest dispatch failed" );
				bFailed = true;
				break;
			}
			SIRResourceHandle handle = {};
			if ( loader.poll( &handle ) != SIR_RESOURCE_RESULT_SUCCESS ) {
				state.SkipWithError( "GRDG ingest publish failed" );
				bFailed = true;
				break;
			}
			++nRevision;
			graph.getWriter()->collectRetired( UINT32_MAX );
			benchmark::DoNotOptimize( handle );
		}
		if ( !bFailed ) {
			state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
			state.counters["committed_bytes"] = ( double )graph.getStats().m_nCommittedBytes;
		}
		if ( !scheduler.shutdown() ) {
			state.SkipWithError( "GRDG ingest scheduler shutdown failed" );
		}
	}
	memory.shutdown();
	if ( allocator.nOutstanding.load() != 0 ) {
		state.SkipWithError( "GRDG ingest leaked resource allocation" );
	}
}

static void SIRBenchmarkGRDGPageRetireCancel( benchmark::State &state )
{
	SIRMemoryManager memory;
	if ( !sirGRDGBenchmarkMemory( memory ) ) {
		state.SkipWithError( "GRDG page benchmark memory init failed" );
		return;
	}
	const uint64_t nPage = memory.getPageSize();
	const uint64_t nBytes = nPage * ( uint64_t )state.range( 0 );
	SIRMemorySegment *pSegment = memory.createSegment( "bench.pages", nBytes * 2 );
	if ( pSegment == nullptr || !pSegment->bindRetirementOwner( 17, 1 ) || !pSegment->commit( 0, nBytes ) ) {
		state.SkipWithError( "GRDG page benchmark segment setup failed" );
		memory.shutdown();
		return;
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		if ( !pSegment->retireIdleRange( 0, nBytes, 17, 1 ) || !pSegment->cancelRetiredRange( 0, nBytes, 17, 1 ) ) {
			state.SkipWithError( "Pending page retirement/cancellation failed" );
			bFailed = true;
			break;
		}
	}
	if ( !bFailed ) {
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
	memory.shutdown();
}

static void SIRBenchmarkGRDGPageDrainRecommit( benchmark::State &state )
{
	SIRMemoryManager memory;
	if ( !sirGRDGBenchmarkMemory( memory ) ) {
		state.SkipWithError( "GRDG page drain memory init failed" );
		return;
	}
	const uint64_t nPage = memory.getPageSize();
	const uint64_t nBytes = nPage * ( uint64_t )state.range( 0 );
	SIRMemorySegment *pSegment = memory.createSegment( "bench.drain", nBytes * 2 );
	if ( pSegment == nullptr || !pSegment->bindRetirementOwner( 18, 1 ) || !pSegment->commit( 0, nBytes ) ) {
		state.SkipWithError( "GRDG page drain setup failed" );
		memory.shutdown();
		return;
	}
	bool bFailed = false;
	for ( auto _ : state ) {
		( void )_;
		if ( !pSegment->retireIdleRange( 0, nBytes, 18, 1 ) ) {
			state.SkipWithError( "GRDG page retirement failed" );
			bFailed = true;
			break;
		}
		const sirPageDrainReport_t result = memory.drainPendingDecommits( nBytes );
		if ( result.nDecommittedBytes != nBytes || !pSegment->commit( 0, nBytes ) ) {
			state.SkipWithError( "GRDG page decommit/recommit failed" );
			bFailed = true;
			break;
		}
	}
	if ( !bFailed ) {
		state.SetBytesProcessed( state.iterations() * ( int64_t )nBytes );
	}
	memory.shutdown();
}

BENCHMARK( SIRBenchmarkGRDGNativeReadAt )->Arg( 4096 )->Arg( 65536 )->Arg( 1048576 );
BENCHMARK( SIRBenchmarkGRDGMappedCopy )->Arg( 4096 )->Arg( 65536 )->Arg( 1048576 );
BENCHMARK( SIRBenchmarkGRDGLookup )->Arg( 32 )->Arg( 256 )->Arg( 1024 );
BENCHMARK( SIRBenchmarkGRDGPublishReplace )->Arg( 64 )->Arg( 4096 )->Arg( 65536 );
BENCHMARK( SIRBenchmarkGRDGFileIngest )
    ->Args( { 4096, 0 } )
    ->Args( { 4096, 1 } )
    ->Args( { 65536, 0 } )
    ->Args( { 65536, 1 } )
    ->Args( { 1048576, 0 } )
    ->Args( { 1048576, 1 } );
BENCHMARK( SIRBenchmarkGRDGPageRetireCancel )->Arg( 1 )->Arg( 16 );
BENCHMARK( SIRBenchmarkGRDGPageDrainRecommit )->Arg( 1 )->Arg( 16 );

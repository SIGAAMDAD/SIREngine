
#include <Resources/SIRResourceGraph.h>
#include <Resources/SIRSystemResourceViews.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <malloc.h>
#endif

struct SIRTestAllocatorStats
{
	std::atomic<int32_t> m_nOutstanding;
	std::atomic<uint64_t> m_nOutstandingBytes;
	std::atomic<bool> m_bFailNext;
};

static void *testAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	SIRTestAllocatorStats *pStats = ( SIRTestAllocatorStats * )pContext;
	if ( pStats->m_bFailNext.exchange( false ) ) {
		return nullptr;
	}
	if ( !nBytes || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nActualAlignment = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nActualAlignment, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory ) {
		pStats->m_nOutstanding.fetch_add( 1 );
		pStats->m_nOutstandingBytes.fetch_add( nBytes );
	}
	return pMemory;
}

static void testFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nAlignment;
	( void )nDomain;
	SIRTestAllocatorStats *pStats = ( SIRTestAllocatorStats * )pContext;
	pStats->m_nOutstanding.fetch_sub( 1 );
	pStats->m_nOutstandingBytes.fetch_sub( nBytes );
#ifdef _WIN32
	_aligned_free( pData );
#else
	free( pData );
#endif
}

#define REQUIRE( condition )                                                                                                               \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d failure: %s\n", __FILE__, __LINE__, #condition );                                                      \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

static SIRResourceGraphConfig testConfig( SIRTestAllocatorStats &rStats, uint32_t nSlots = 64, uint64_t nBudget = 4 * 1024 * 1024 ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &rStats, testAllocate, testFree };
	config.m_nMaxResources = nSlots;
	config.m_nMaxStaging = 64;
	config.m_nMaxProviderPins = 32;
	config.m_nMetadataDomain = 7;
	config.m_nMaxCommittedBytes = nBudget;
	return config;
}

static SIRResourceKey key( uint64_t nAsset, uint32_t nType = 21 ) noexcept
{
	return { nAsset, nType, 0 };
}

static SIRResourceContentKey content( uint64_t nHash ) noexcept
{
	return { nHash, nHash ^ 0x9E3779B97F4A7C15ULL, 4, 0 };
}

static SIRResourceHandle publishText( SIRResourceGraphWriter &rWriter, SIRResourceKey resourceKey, const char *szData, uint64_t nHash,
                                      SIRResourceChunkId chunkId = { 0 } ) noexcept
{
	SIRResourceStagingHandle staging = {};
	SIRResourceMutableBytes bytes = {};
	const uint64_t nBytes = strlen( szData ) + 1;
	REQUIRE( rWriter.allocateStaging( nBytes, 8, 9, &staging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memcpy( bytes.m_pData, szData, ( size_t )nBytes );
	SIRResourceChunkPublication chunk = { chunkId, content( nHash ), staging, false };
	SIRResourcePublication pub = { resourceKey, SIR_RESOURCE_KIND_BLOB, 1, 4, &chunk, 1, nullptr, 0 };
	SIRResourceHandle result = {};
	REQUIRE( rWriter.publish( pub, &result ) == SIR_RESOURCE_RESULT_SUCCESS );
	return result;
}

static void testScopeLifetimeAndDeduplication( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		const SIRResourceHandle first = publishText( *pWriter, key( 10 ), "one", 10 );
		const SIRResourceHandle second = publishText( *pWriter, key( 20 ), "one", 10 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
		REQUIRE( graph.getStats().m_nPayloadBytes == 4 );
		const uint64_t nBefore = graph.getStats().m_nCommittedBytes;
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView v1;
			SIRResourceView v2;
			REQUIRE( graph.resolve( scope, first, &v1 ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( graph.resolve( scope, second, &v2 ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceConstBytes bytes1 = {};
			SIRResourceConstBytes bytes2 = {};
			REQUIRE( v1.getChunk( { 0 }, &bytes1 ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( v2.getChunk( { 0 }, &bytes2 ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( bytes1.m_pData == bytes2.m_pData );
			REQUIRE( strcmp( ( const char * )bytes1.m_pData, "one" ) == 0 );
			const SIRResourceHandle updated = publishText( *pWriter, key( 10 ), "two", 10 );
			REQUIRE( updated.m_nGeneration == first.m_nGeneration );
			REQUIRE( v1.getRevision() == 1 );
			REQUIRE( strcmp( ( const char * )bytes1.m_pData, "one" ) == 0 );
			REQUIRE( pWriter->collectRetired( 10 ) == 0 );
			SIRResourceView latest;
			REQUIRE( graph.resolve( scope, first, &latest ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( latest.getRevision() == 2 );
			SIRResourceConstBytes newer = {};
			REQUIRE( latest.getChunk( { 0 }, &newer ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( strcmp( ( const char * )newer.m_pData, "two" ) == 0 );
			REQUIRE( bytes1.m_pData != newer.m_pData );
			REQUIRE( graph.getStats().m_nUniqueChunks == 2 );
		}
		REQUIRE( pWriter->collectRetired( 10 ) == 1 );
		REQUIRE( graph.getStats().m_nCommittedBytes < nBefore + 2048 );
		REQUIRE( pWriter->retire( second ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 10 ) == 1 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
		REQUIRE( pWriter->retire( first ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 10 ) == 1 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 0 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testGenerationAndStagingRollback( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 1 ) );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		SIRResourceHandle first = {};
		REQUIRE( pWriter->request( key( 1 ), &first ) == SIR_RESOURCE_RESULT_SUCCESS );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceHandle pending = {};
			REQUIRE( graph.find( scope, key( 1 ), &pending ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( pending.m_nGeneration == first.m_nGeneration );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, pending, &view ) == SIR_RESOURCE_RESULT_NOT_RESIDENT );
		}
		REQUIRE( pWriter->markFailed( first, SIR_RESOURCE_RESULT_INVALID_FORMAT ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 1 ) == 1 );
		REQUIRE( pWriter->retire( first ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceHandle second = {};
		REQUIRE( pWriter->request( key( 2 ), &second ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( first.m_nSlot == second.m_nSlot && first.m_nGeneration != second.m_nGeneration );
		SIRResourceReadScope scope( graph );
		SIRResourceView view;
		REQUIRE( graph.resolve( scope, first, &view ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( pWriter->collectRetired( 1 ) == 0 );
		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes bytes = {};
		REQUIRE( pWriter->allocateStaging( 64, 8, 4, &staging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceChunkPublication a = { { 1 }, content( 1 ), staging, false };
		SIRResourceChunkPublication b = { { 1 }, content( 2 ), staging, false };
		SIRResourceChunkPublication pair[] = { a, b };
		SIRResourcePublication invalid = { key( 2 ), SIR_RESOURCE_KIND_BLOB, 1, 4, pair, 2, nullptr, 0 };
		SIRResourceHandle unused = {};
		REQUIRE( pWriter->publish( invalid, &unused ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( pWriter->cancelStaging( staging ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->cancelStaging( staging ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testPinsEvictionAndDependencies( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc ) );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		SIRResourceHandle mesh = publishText( *pWriter, key( 10 ), "mesh", 10, { 7 } );
		SIRResourceDependency edge = { key( 10 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		SIRResourcePublication scene = { key( 20 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, &edge, 1 };
		SIRResourceHandle hScene = {};
		REQUIRE( pWriter->publish( scene, &hScene ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency cycle = { key( 20 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		SIRResourcePublication bad = { key( 10 ), SIR_RESOURCE_KIND_BLOB, 1, 1, nullptr, 0, &cycle, 1 };
		SIRResourceHandle ignored = {};
		REQUIRE( pWriter->publish( bad, &ignored ) == SIR_RESOURCE_RESULT_DEPENDENCY_CYCLE );
		SIRResourcePinHandle pin = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, mesh, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( graph.pinForProvider( scope, view, 4, &pin ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceView sceneView;
			SIRResourceView childView;
			REQUIRE( graph.resolve( scope, hScene, &sceneView ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( graph.resolveDependency( scope, sceneView, 0, &childView ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( childView.getHandle().m_nSlot == mesh.m_nSlot );
		}
		REQUIRE( pWriter->evictChunk( mesh, { 7 } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 9 ) == 0 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
		REQUIRE( graph.releaseProviderPin( pin ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.releaseProviderPin( pin ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( pWriter->collectRetired( 9 ) == 1 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 0 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testBudgetFailureAndMissingDependencies( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc ) );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		SIRResourceDependency missing = { key( 999 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		SIRResourcePublication pub = { key( 12 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, &missing, 1 };
		SIRResourceHandle h = {};
		REQUIRE( pWriter->publish( pub, &h ) == SIR_RESOURCE_RESULT_DEPENDENCY_MISSING );
		REQUIRE( graph.getStats().m_nLiveResources == 0 );

		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes bytes = {};
		REQUIRE( pWriter->allocateStaging( 16, 8, 9, &staging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		memset( bytes.m_pData, 7, 16 );
		SIRResourceChunkPublication chunk = { { 0 }, content( 700 ), staging, false };
		SIRResourcePublication valid = { key( 12 ), SIR_RESOURCE_KIND_BLOB, 1, 1, &chunk, 1, nullptr, 0 };
		const uint64_t nBefore = graph.getStats().m_nCommittedBytes;
		alloc.m_bFailNext = true;
		REQUIRE( pWriter->publish( valid, &h ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( graph.getStats().m_nCommittedBytes == nBefore );
		REQUIRE( pWriter->publish( valid, &h ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );

		SIRResourceChunkPublication reuse = { { 3 }, content( 700 ), {}, true };
		SIRResourcePublication shared = { key( 15 ), SIR_RESOURCE_KIND_BLOB, 1, 1, &reuse, 1, nullptr, 0 };
		SIRResourceHandle other = {};
		REQUIRE( pWriter->publish( shared, &other ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
		REQUIRE( pWriter->retire( h ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 1 ) == 1 );
		REQUIRE( graph.getStats().m_nUniqueChunks == 1 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testDeepCycleWithoutRecursion( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 260 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		SIRResourcePublication root = { key( 1 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		SIRResourceHandle h = {};
		REQUIRE( pWriter->publish( root, &h ) == SIR_RESOURCE_RESULT_SUCCESS );
		for ( uint64_t nAsset = 2; nAsset <= 201; ++nAsset ) {
			SIRResourceDependency edge = { key( nAsset - 1 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
			SIRResourcePublication next = { key( nAsset ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, &edge, 1 };
			REQUIRE( pWriter->publish( next, &h ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		SIRResourceDependency cycle = { key( 201 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		SIRResourcePublication invalid = { key( 1 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, &cycle, 1 };
		REQUIRE( pWriter->publish( invalid, &h ) == SIR_RESOURCE_RESULT_DEPENDENCY_CYCLE );
		REQUIRE( graph.getStats().m_nLiveResources == 201 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testConcurrentReadersAndWriterBoundary( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc ) );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter );
		SIRResourceHandle h = publishText( *pWriter, key( 55 ), "data-0", 55 );
		std::atomic<bool> bStart{ false };
		std::atomic<int32_t> nErrors{ 0 };
		std::vector<std::thread> threads;
		for ( uint32_t iReader = 0; iReader < 6; ++iReader ) {
			threads.emplace_back( [&]() {
				while ( !bStart.load( std::memory_order_acquire ) ) {
					std::this_thread::yield();
				}
				if ( graph.getWriter() != nullptr ) {
					++nErrors;
				}
				for ( uint32_t iPass = 0; iPass < 5000; ++iPass ) {
					SIRResourceReadScope scope( graph );
					SIRResourceView view;
					if ( graph.resolve( scope, h, &view ) != SIR_RESOURCE_RESULT_SUCCESS ) {
						++nErrors;
						continue;
					}
					SIRResourceConstBytes bytes = {};
					if ( view.getChunk( { 0 }, &bytes ) != SIR_RESOURCE_RESULT_SUCCESS || memcmp( bytes.m_pData, "data-", 5 ) != 0 ) {
						++nErrors;
					}
				}
			} );
		}
		bStart.store( true, std::memory_order_release );
		for ( uint32_t iPass = 1; iPass <= 400; ++iPass ) {
			char szData[32] = {};
			snprintf( szData, sizeof( szData ), "data-%u", iPass );
			publishText( *pWriter, key( 55 ), szData, 55 + iPass );
			pWriter->collectRetired( 4 );
		}
		for ( std::thread &rReader : threads ) {
			rReader.join();
		}
		REQUIRE( nErrors.load() == 0 );
		pWriter->collectRetired( UINT32_MAX );
		REQUIRE( graph.getStats().m_nRetiredRecords == 0 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testIndexedKeyLookupAndGenerationReuse( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 96 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourceKey keys[96] = {};
		SIRResourceHandle handles[96] = {};
		for ( uint32_t iKey = 0; iKey < 96; ++iKey ) {
			keys[iKey] = { 100 + ( iKey % 16 ), 1 + ( iKey / 16 ) % 3, iKey / 48 };
			REQUIRE( pWriter->request( keys[iKey], &handles[iKey] ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( graph.getStats().m_nLiveResources == 96 );
		{
			SIRResourceReadScope scope( graph );
			for ( uint32_t iKey = 0; iKey < 96; ++iKey ) {
				SIRResourceHandle found = {};
				REQUIRE( graph.find( scope, keys[iKey], &found ) == SIR_RESOURCE_RESULT_SUCCESS );
				REQUIRE( found.m_nSlot == handles[iKey].m_nSlot );
			}
		}
		for ( uint32_t iKey = 0; iKey < 96; iKey += 2 ) {
			REQUIRE( pWriter->retire( handles[iKey] ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( graph.getStats().m_nLiveResources == 48 );
		REQUIRE( pWriter->collectRetired( 96 ) == 48 );
		for ( uint32_t iKey = 0; iKey < 48; ++iKey ) {
			SIRResourceHandle replacement = {};
			REQUIRE( pWriter->request( key( 10000 + iKey ), &replacement ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceReadScope scope( graph );
			SIRResourceView stale;
			REQUIRE( graph.resolve( scope, handles[iKey * 2], &stale ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		}
		REQUIRE( graph.getStats().m_nLiveResources == 96 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

int main( void )
{
	testScopeLifetimeAndDeduplication();
	testGenerationAndStagingRollback();
	testPinsEvictionAndDependencies();
	testBudgetFailureAndMissingDependencies();
	testDeepCycleWithoutRecursion();
	testConcurrentReadersAndWriterBoundary();
	testIndexedKeyLookupAndGenerationReuse();
	printf( "PASS: GRDG resource registry, publication, deduplication, lifetime, concurrency\n" );
	return 0;
}

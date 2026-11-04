
#include <Resources/SIRResourceGraph.h>
#include <Core/Jobs/SIRTaskGraph.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
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
		// This scope entered after the first resource retired, so it cannot
		// hold the old immutable record and collection is safe.
		REQUIRE( pWriter->collectRetired( 1 ) == 1 );
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
		SIRResourceDependencyPlanEntry order[201] = {};
		uint32_t nRequired = 0;
		REQUIRE( pWriter->compileDependencyOrder( h, order, 201, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nRequired == 201 );
		for ( uint32_t iNode = 0; iNode < nRequired; ++iNode ) {
			REQUIRE( order[iNode].hResource.m_nSlot == iNode );
			REQUIRE( order[iNode].nRevision == 1 );
		}
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

static void testExpectedRevisionPublication( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 2 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourcePublication publication = { key( 500 ), SIR_RESOURCE_KIND_BLOB, 1, 4, nullptr, 0, nullptr, 0 };
		SIRResourceHandle handle = {};
		REQUIRE( pWriter->publishExpected( publication, 0, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nLiveResources == 1 );
		SIRResourceStagingHandle staging = {};
		SIRResourceMutableBytes stagedBytes = {};
		REQUIRE( pWriter->allocateStaging( 8, 8, 9, &staging, &stagedBytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		memset( stagedBytes.m_pData, 0x31, 8 );
		SIRResourceChunkPublication chunk = { { 0 }, content( 500 ), staging, false };
		publication.m_pChunks = &chunk;
		publication.m_nChunkCount = 1;
		SIRResourceHandle unused = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
		const uint64_t nBudgetBefore = graph.getStats().m_nCommittedBytes;
		REQUIRE( pWriter->publishExpected( publication, 0, &unused ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		REQUIRE( unused.m_nSlot == UINT32_MAX );
		REQUIRE( graph.getStats().m_nCommittedBytes == nBudgetBefore );
		REQUIRE( pWriter->cancelStaging( staging ) == SIR_RESOURCE_RESULT_SUCCESS );
		publication.m_pChunks = nullptr;
		publication.m_nChunkCount = 0;
		REQUIRE( pWriter->publishExpected( publication, 1, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( view.getRevision() == 2 );
		}
		REQUIRE( pWriter->retire( handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->publishExpected( publication, 2, &unused ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		REQUIRE( pWriter->publishExpected( publication, 0, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, handle, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( view.getRevision() == 1 );
		}
		REQUIRE( pWriter->retire( handle ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 3 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testReverseDependenciesAndTransitiveInvalidation( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 16 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourcePublication base = { key( 1 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		SIRResourceHandle hA = {};
		SIRResourceHandle hD = {};
		SIRResourceHandle hB = {};
		SIRResourceHandle hC = {};
		SIRResourceHandle hE = {};
		SIRResourceHandle hMissing = {};
		REQUIRE( pWriter->publish( base, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		base.m_Key = key( 4 );
		REQUIRE( pWriter->publish( base, &hD ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onA[2] = { { key( 1 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                 { key( 1 ), SIR_RESOURCE_EDGE_KIND_OPTIONAL, {} } };
		base.m_Key = key( 2 );
		base.m_pDependencies = onA;
		base.m_nDependencyCount = 2;
		REQUIRE( pWriter->publish( base, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onB = { key( 2 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		base.m_Key = key( 3 );
		base.m_pDependencies = &onB;
		base.m_nDependencyCount = 1;
		REQUIRE( pWriter->publish( base, &hC ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onMissing = { key( 999 ), SIR_RESOURCE_EDGE_KIND_OPTIONAL, {} };
		base.m_Key = key( 5 );
		base.m_pDependencies = &onMissing;
		REQUIRE( pWriter->publish( base, &hE ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 4 );

		uint32_t nRequired = 0;
		REQUIRE( pWriter->collectDependents( key( 1 ), nullptr, 0, &nRequired ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( nRequired == 1 ); // Duplicate edges must not duplicate handles.
		SIRResourceHandle found[2] = {};
		REQUIRE( pWriter->collectDependents( key( 1 ), found, 2, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nRequired == 1 && found[0].m_nSlot == hB.m_nSlot );
		REQUIRE( pWriter->collectDependents( key( 2 ), found, 2, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nRequired == 1 && found[0].m_nSlot == hC.m_nSlot );

		uint64_t nB = UINT64_MAX;
		uint64_t nC = UINT64_MAX;
		uint64_t nD = UINT64_MAX;
		uint64_t nE = UINT64_MAX;
		REQUIRE( pWriter->getInvalidationSequence( hB, &nB ) == SIR_RESOURCE_RESULT_SUCCESS && nB == 0 );
		REQUIRE( pWriter->getInvalidationSequence( hC, &nC ) == SIR_RESOURCE_RESULT_SUCCESS && nC == 0 );
		base = { key( 1 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		REQUIRE( pWriter->publishExpected( base, 1, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->getInvalidationSequence( hB, &nB ) == SIR_RESOURCE_RESULT_SUCCESS && nB > 0 );
		REQUIRE( pWriter->getInvalidationSequence( hC, &nC ) == SIR_RESOURCE_RESULT_SUCCESS && nC == nB );
		REQUIRE( pWriter->getInvalidationSequence( hD, &nD ) == SIR_RESOURCE_RESULT_SUCCESS && nD == 0 );
		REQUIRE( pWriter->getInvalidationSequence( hE, &nE ) == SIR_RESOURCE_RESULT_SUCCESS && nE == 0 );

		SIRResourceDependency onD = { key( 4 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		base.m_Key = key( 2 );
		base.m_pDependencies = &onD;
		base.m_nDependencyCount = 1;
		REQUIRE( pWriter->publishExpected( base, 1, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 3 );
		REQUIRE( pWriter->getInvalidationSequence( hB, &nB ) == SIR_RESOURCE_RESULT_SUCCESS && nB == 0 );
		REQUIRE( pWriter->collectDependents( key( 1 ), found, 2, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS && nRequired == 0 );
		REQUIRE( pWriter->collectDependents( key( 4 ), found, 2, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nRequired == 1 && found[0].m_nSlot == hB.m_nSlot );

		REQUIRE( pWriter->retire( hD ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->getInvalidationSequence( hB, &nB ) == SIR_RESOURCE_RESULT_SUCCESS && nB > 0 );
		REQUIRE( pWriter->getInvalidationSequence( hC, &nC ) == SIR_RESOURCE_RESULT_SUCCESS && nC == nB );
		base = { key( 999 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		REQUIRE( pWriter->publish( base, &hMissing ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->getInvalidationSequence( hE, &nE ) == SIR_RESOURCE_RESULT_SUCCESS && nE > 0 );
		REQUIRE( pWriter->retire( hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->getInvalidationSequence( hB, &nB ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( pWriter->collectDependents( key( 4 ), found, 2, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS && nRequired == 0 );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 2 );

		std::atomic<uint32_t> eOther{ 0 };
		std::thread other( [&]() {
			uint32_t nTotal = 0;
			eOther.store( pWriter->collectDependents( key( 999 ), nullptr, 0, &nTotal ) );
		} );
		other.join();
		REQUIRE( eOther.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testReverseDependencyCapacityRollback( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraphConfig config = testConfig( alloc, 6 );
		config.m_nMaxDependencyEdges = 1;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourcePublication pub = { key( 40 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		SIRResourceHandle hA = {};
		SIRResourceHandle hB = {};
		SIRResourceHandle hC = {};
		SIRResourceHandle hD = {};
		REQUIRE( pWriter->publish( pub, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		pub.m_Key = key( 41 );
		REQUIRE( pWriter->publish( pub, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onA = { key( 40 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		pub.m_Key = key( 42 );
		pub.m_pDependencies = &onA;
		pub.m_nDependencyCount = 1;
		REQUIRE( pWriter->publish( pub, &hC ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onB = { key( 41 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		pub.m_Key = key( 43 );
		pub.m_pDependencies = &onB;
		REQUIRE( pWriter->publish( pub, &hD ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 1 );
		REQUIRE( graph.getStats().m_nLiveResources == 3 );
		REQUIRE( pWriter->retire( hC ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 0 );
		REQUIRE( pWriter->publish( pub, &hD ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 1 );
		REQUIRE( pWriter->retire( hD ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nIndexedDependencyEdges == 0 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testInvalidationJournalOverflowAndResume( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraphConfig config = testConfig( alloc, 8 );
		config.m_nMaxInvalidationEvents = 2;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourceHandle hA = {};
		SIRResourceHandle hB = {};
		SIRResourceHandle hC = {};
		SIRResourcePublication publication = { key( 500 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		REQUIRE( pWriter->publish( publication, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onA = { key( 500 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		publication.m_Key = key( 501 );
		publication.m_pDependencies = &onA;
		publication.m_nDependencyCount = 1;
		REQUIRE( pWriter->publish( publication, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency onB = { key( 501 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		publication.m_Key = key( 502 );
		publication.m_pDependencies = &onB;
		REQUIRE( pWriter->publish( publication, &hC ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nTotalInvalidationEvents == 0 );

		publication = { key( 500 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		REQUIRE( pWriter->publishExpected( publication, 1, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceGraphStats stats = graph.getStats();
		REQUIRE( stats.m_nTotalInvalidationEvents == 2 && stats.m_nBufferedInvalidationEvents == 2 );
		SIRResourceInvalidationEvent events[2] = {};
		uint32_t nWritten = 0;
		uint64_t nCursor = UINT64_MAX;
		REQUIRE( pWriter->pollInvalidations( 0, events, 1, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nWritten == 1 && nCursor == 1 && events[0].m_nJournalSequence == 1 );
		REQUIRE( events[0].m_Dependent.m_nSlot == hB.m_nSlot && events[0].m_Cause.m_nAssetId == 500 );
		REQUIRE( pWriter->pollInvalidations( 1, events, 1, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nWritten == 1 && nCursor == 2 && events[0].m_nJournalSequence == 2 );
		REQUIRE( events[0].m_Dependent.m_nSlot == hC.m_nSlot );
		REQUIRE( pWriter->pollInvalidations( 2, events, 2, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nWritten == 0 && nCursor == 2 );
		REQUIRE( pWriter->publishExpected( publication, 2, &hA ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.getStats().m_nTotalInvalidationEvents == 4 );
		REQUIRE( pWriter->pollInvalidations( 1, events, 2, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_JOURNAL_OVERFLOW );
		REQUIRE( nWritten == 0 && nCursor == 4 );
		REQUIRE( pWriter->pollInvalidations( 2, events, 2, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nWritten == 2 && nCursor == 4 );
		REQUIRE( events[0].m_nJournalSequence == 3 && events[1].m_nJournalSequence == 4 );
		REQUIRE( pWriter->pollInvalidations( 4, nullptr, 1, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		REQUIRE( pWriter->pollInvalidations( 5, events, 2, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );

		publication = { key( 501 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, &onA, 1 };
		REQUIRE( pWriter->publishExpected( publication, 1, &hB ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->pollInvalidations( 4, events, 2, &nWritten, &nCursor ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nWritten == 1 && nCursor == 5 );
		REQUIRE( events[0].m_Dependent.m_nSlot == hC.m_nSlot && events[0].m_Cause.m_nAssetId == 501 );
		REQUIRE( events[0].m_nChangeSequence == graph.getStats().m_nChangeSequence );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testAtomicReadIndexCollisionsAndChurn( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 64 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const uint32_t nBucketCount = 129;
		const uint32_t nCollisionBucket = ( uint32_t )( SIRResourceKeyHash{}( key( 1000 ) ) % nBucketCount );
		SIRResourceKey keys[8] = {};
		SIRResourceHandle handles[8] = {};
		uint32_t nKeys = 0;
		for ( uint64_t nAsset = 1000; nAsset < 100000 && nKeys < 8; ++nAsset ) {
			SIRResourceKey candidate = key( nAsset );
			if ( SIRResourceKeyHash{}( candidate ) % nBucketCount == nCollisionBucket ) {
				keys[nKeys++] = candidate;
			}
		}
		REQUIRE( nKeys == 8 );
		for ( uint32_t i = 0; i < nKeys; ++i ) {
			SIRResourcePublication publication = { keys[i], SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
			REQUIRE( pWriter->publish( publication, &handles[i] ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		{
			SIRResourceReadScope scope( graph );
			for ( uint32_t i = 0; i < nKeys; ++i ) {
				SIRResourceHandle hFound = {};
				REQUIRE( graph.find( scope, keys[i], &hFound ) == SIR_RESOURCE_RESULT_SUCCESS );
				REQUIRE( hFound.m_nSlot == handles[i].m_nSlot && hFound.m_nGeneration == handles[i].m_nGeneration );
			}
		}
		for ( uint32_t i = 0; i < nKeys; i += 2 ) {
			REQUIRE( pWriter->retire( handles[i] ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		{
			SIRResourceReadScope scope( graph );
			for ( uint32_t i = 0; i < nKeys; ++i ) {
				SIRResourceHandle hFound = {};
				const SIRResourceResult eExpected = i % 2 == 0 ? SIR_RESOURCE_RESULT_INVALID_HANDLE : SIR_RESOURCE_RESULT_SUCCESS;
				REQUIRE( graph.find( scope, keys[i], &hFound ) == eExpected );
			}
		}
		SIRResourceKey replacements[4] = {};
		SIRResourceHandle replacementHandles[4] = {};
		uint32_t nReplacement = 0;
		for ( uint64_t nAsset = 100001; nAsset < 200000 && nReplacement < 4; ++nAsset ) {
			SIRResourceKey candidate = key( nAsset );
			if ( SIRResourceKeyHash{}( candidate ) % nBucketCount == nCollisionBucket ) {
				replacements[nReplacement] = candidate;
				SIRResourcePublication publication = { candidate, SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
				REQUIRE( pWriter->publish( publication, &replacementHandles[nReplacement] ) == SIR_RESOURCE_RESULT_SUCCESS );
				++nReplacement;
			}
		}
		REQUIRE( nReplacement == 4 );
		{
			SIRResourceReadScope scope( graph );
			for ( uint32_t i = 0; i < nReplacement; ++i ) {
				SIRResourceHandle hFound = {};
				REQUIRE( graph.find( scope, replacements[i], &hFound ) == SIR_RESOURCE_RESULT_SUCCESS );
				REQUIRE( hFound.m_nGeneration == replacementHandles[i].m_nGeneration );
			}
			for ( uint32_t i = 1; i < nKeys; i += 2 ) {
				SIRResourceHandle hFound = {};
				REQUIRE( graph.find( scope, keys[i], &hFound ) == SIR_RESOURCE_RESULT_SUCCESS );
			}
		}

		std::atomic<bool> bRunning{ true };
		std::atomic<uint32_t> nFailures{ 0 };
		std::thread readers[4];
		for ( std::thread &rThread : readers ) {
			rThread = std::thread( [&]() {
				while ( bRunning.load( std::memory_order_acquire ) ) {
					SIRResourceReadScope scope( graph );
					SIRResourceHandle hFound = {};
					if ( graph.find( scope, keys[1], &hFound ) != SIR_RESOURCE_RESULT_SUCCESS || hFound.m_nSlot != handles[1].m_nSlot ) {
						nFailures.fetch_add( 1, std::memory_order_relaxed );
					}
					SIRResourceView view;
					if ( graph.resolve( scope, handles[1], &view ) != SIR_RESOURCE_RESULT_SUCCESS ) {
						nFailures.fetch_add( 1, std::memory_order_relaxed );
					}
				}
			} );
		}
		for ( uint32_t i = 0; i < 100; ++i ) {
			SIRResourcePublication publication = { keys[1], SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
			REQUIRE( pWriter->publishExpected( publication, i + 1, &handles[1] ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( pWriter->retire( replacementHandles[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( pWriter->publish( { replacements[0], SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 },
			                           &replacementHandles[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
			( void )pWriter->collectRetired( 10 );
		}
		bRunning.store( false, std::memory_order_release );
		for ( std::thread &rThread : readers ) {
			rThread.join();
		}
		REQUIRE( nFailures.load() == 0 );
		REQUIRE( graph.getStats().m_nLiveResources == 8 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testEpochReclamationWithOverlappingScopes( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraphConfig config = testConfig( alloc );
		config.nMaxReaders = 3;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );

		const SIRResourceHandle hResource = publishText( *pWriter, key( 700 ), "version-one", 1700 );
		alignas( SIRResourceReadScope ) unsigned char szOldScope[sizeof( SIRResourceReadScope )];
		SIRResourceReadScope *pOldScope = new ( szOldScope ) SIRResourceReadScope( graph );
		REQUIRE( pOldScope->isActive() );
		SIRResourceView oldView;
		REQUIRE( graph.resolve( *pOldScope, hResource, &oldView ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceConstBytes oldBytes = {};
		REQUIRE( oldView.getChunk( { 0 }, &oldBytes ) == SIR_RESOURCE_RESULT_SUCCESS );

		publishText( *pWriter, key( 700 ), "version-two", 1701 );
		SIRResourcePinHandle hProviderPin = {};
		{
			SIRResourceReadScope newerScope( graph );
			REQUIRE( newerScope.isActive() );
			SIRResourceView newerView;
			REQUIRE( graph.resolve( newerScope, hResource, &newerView ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( newerView.getRevision() == 2 );
			SIRResourceConstBytes newerBytes = {};
			REQUIRE( newerView.getChunk( { 0 }, &newerBytes ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( strcmp( ( const char * )newerBytes.m_pData, "version-two" ) == 0 );
			REQUIRE( graph.pinForProvider( newerScope, newerView, 9, &hProviderPin ) == SIR_RESOURCE_RESULT_SUCCESS );

			publishText( *pWriter, key( 700 ), "version-three", 1702 );
			REQUIRE( graph.getStats().nPublicationEpoch >= 4 );
			REQUIRE( graph.getStats().m_nRetiredRecords == 2 );
			REQUIRE( graph.getStats().m_nActiveReaders == 2 );
			REQUIRE( pWriter->collectRetired( 8 ) == 0 );
			REQUIRE( strcmp( ( const char * )oldBytes.m_pData, "version-one" ) == 0 );

			pOldScope->~SIRResourceReadScope();
			REQUIRE( graph.getStats().m_nActiveReaders == 1 );
			REQUIRE( pWriter->collectRetired( 8 ) == 1 );
			REQUIRE( graph.getStats().m_nRetiredRecords == 1 );
			REQUIRE( strcmp( ( const char * )newerBytes.m_pData, "version-two" ) == 0 );
		}
		REQUIRE( graph.getStats().m_nActiveReaders == 0 );
		REQUIRE( pWriter->collectRetired( 8 ) == 0 );
		REQUIRE( graph.getStats().m_nRetiredRecords == 1 );
		REQUIRE( graph.releaseProviderPin( hProviderPin ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
		REQUIRE( graph.getStats().m_nRetiredRecords == 0 );
		REQUIRE( pWriter->retire( hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->collectRetired( 8 ) == 1 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testReaderEpochSlotExhaustion( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraphConfig config = testConfig( alloc );
		config.nMaxReaders = 2;
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		REQUIRE( graph.getStats().nMaxReaders == 2 );
		{
			SIRResourceReadScope first( graph );
			SIRResourceReadScope second( graph );
			SIRResourceReadScope exhausted( graph );
			REQUIRE( first.isActive() && second.isActive() );
			REQUIRE( !exhausted.isActive() );
			REQUIRE( graph.getStats().m_nActiveReaders == 2 );
			SIRResourceHandle hMissing = {};
			REQUIRE( graph.find( exhausted, key( 123 ), &hMissing ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
		}
		REQUIRE( graph.getStats().m_nActiveReaders == 0 );
		SIRResourceReadScope recovered( graph );
		REQUIRE( recovered.isActive() );
		REQUIRE( graph.getStats().m_nActiveReaders == 1 );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testDeterministicDependencyOrderCompilation( void )
{
	SIRTestAllocatorStats alloc = {};
	{
		SIRResourceGraph graph( testConfig( alloc, 16 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		SIRResourceHandle handles[6] = {};
		SIRResourcePublication pub = { key( 1 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, nullptr, 0 };
		REQUIRE( pWriter->publish( pub, &handles[0] ) == SIR_RESOURCE_RESULT_SUCCESS );
		pub.m_Key = key( 2 );
		REQUIRE( pWriter->publish( pub, &handles[1] ) == SIR_RESOURCE_RESULT_SUCCESS );

		SIRResourceDependency edgesC[2] = { { key( 1 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                    { key( 2 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} } };
		pub.m_Key = key( 3 );
		pub.m_pDependencies = edgesC;
		pub.m_nDependencyCount = 2;
		REQUIRE( pWriter->publish( pub, &handles[2] ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency edgesD[2] = { { key( 3 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                    { key( 2 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} } };
		pub.m_Key = key( 4 );
		pub.m_pDependencies = edgesD;
		REQUIRE( pWriter->publish( pub, &handles[3] ) == SIR_RESOURCE_RESULT_SUCCESS );

		SIRResourceDependency edgeE = { key( 999 ), SIR_RESOURCE_EDGE_KIND_OPTIONAL, {} };
		pub.m_Key = key( 5 );
		pub.m_pDependencies = &edgeE;
		pub.m_nDependencyCount = 1;
		REQUIRE( pWriter->publish( pub, &handles[4] ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDependency edgesF[2] = { { key( 4 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                    { key( 5 ), SIR_RESOURCE_EDGE_KIND_STREAMED, {} } };
		pub.m_Key = key( 6 );
		pub.m_pDependencies = edgesF;
		pub.m_nDependencyCount = 2;
		REQUIRE( pWriter->publish( pub, &handles[5] ) == SIR_RESOURCE_RESULT_SUCCESS );

		uint32_t nRequired = UINT32_MAX;
		REQUIRE( pWriter->compileDependencyOrder( handles[5], nullptr, 0, &nRequired ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( nRequired == 6 );
		SIRResourceDependencyPlanEntry tooSmall[2] = {};
		tooSmall[0].hResource.m_nSlot = UINT32_MAX;
		REQUIRE( pWriter->compileDependencyOrder( handles[5], tooSmall, 2, &nRequired ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( nRequired == 6 && tooSmall[0].hResource.m_nSlot == UINT32_MAX );

		SIRResourceDependencyPlanEntry order[6] = {};
		REQUIRE( pWriter->compileDependencyOrder( handles[5], order, 6, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nRequired == 6 );
		for ( uint32_t iNode = 0; iNode < 6; ++iNode ) {
			REQUIRE( order[iNode].hResource.m_nSlot == handles[iNode].m_nSlot );
			REQUIRE( order[iNode].nRevision == 1 && order[iNode].eState == SIR_RESOURCE_STATE_RESIDENT );
		}

		pub = { key( 3 ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, edgesC, 2 };
		REQUIRE( pWriter->publishExpected( pub, 1, &handles[2] ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->compileDependencyOrder( handles[5], order, 6, &nRequired ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( order[2].nRevision == 2 );

		std::atomic<uint32_t> nOtherResult{ 0 };
		std::thread other( [&]() {
			uint32_t nCount = 0;
			nOtherResult.store( pWriter->compileDependencyOrder( handles[5], nullptr, 0, &nCount ) );
		} );
		other.join();
		REQUIRE( nOtherResult.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );

		REQUIRE( pWriter->retire( handles[1] ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->compileDependencyOrder( handles[5], order, 6, &nRequired ) == SIR_RESOURCE_RESULT_DEPENDENCY_MISSING );
		REQUIRE( nRequired == 0 );
		REQUIRE( pWriter->retire( handles[5] ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( pWriter->compileDependencyOrder( handles[5], order, 6, &nRequired ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
	}
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

typedef struct sirResourceScheduledTestNode_s
{
	SIRResourceHandle hResource;
	std::atomic<uint32_t> nRuns;
	const std::atomic<uint32_t> *pPreA;
	const std::atomic<uint32_t> *pPreB;
	bool bFail;
} sirResourceScheduledTestNode_t;

typedef struct sirResourceScheduledTestSet_s
{
	sirResourceScheduledTestNode_t nodes[8];
	uint32_t nNodes;
	bool bRejectFactory;
} sirResourceScheduledTestSet_t;

static sirTaskResult_t executeResourceTest( void *pContext, sirTaskHandle_t ) noexcept
{
	sirResourceScheduledTestNode_t *pNode = ( sirResourceScheduledTestNode_t * )pContext;
	if ( ( pNode->pPreA != nullptr && pNode->pPreA->load( std::memory_order_acquire ) == 0 ) ||
	     ( pNode->pPreB != nullptr && pNode->pPreB->load( std::memory_order_acquire ) == 0 ) ) {
		return SIR_TASK_RESULT_FAILURE;
	}
	pNode->nRuns.fetch_add( 1, std::memory_order_release );
	return pNode->bFail ? SIR_TASK_RESULT_FAILURE : SIR_TASK_RESULT_SUCCESS;
}

static bool describeResourceTest( void *pContext, const SIRResourceDependencyPlanEntry &rEntry, sirTaskGraphPass_t *pPass ) noexcept
{
	sirResourceScheduledTestSet_t *pSet = ( sirResourceScheduledTestSet_t * )pContext;
	if ( pSet->bRejectFactory || rEntry.nRevision == 0 || pPass == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < pSet->nNodes; ++i ) {
		sirResourceScheduledTestNode_t &rNode = pSet->nodes[i];
		if ( rNode.hResource.m_nSlot == rEntry.hResource.m_nSlot && rNode.hResource.m_nGeneration == rEntry.hResource.m_nGeneration ) {
			*pPass = { executeResourceTest, &rNode, SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
			return true;
		}
	}
	return false;
}

static SIRResourceHandle publishTaskNode( SIRResourceGraphWriter &rWriter, uint64_t nAsset,
                                          const SIRResourceDependency *pDependencies = nullptr, uint32_t nDependencies = 0 ) noexcept
{
	SIRResourcePublication pub = { key( nAsset ), SIR_RESOURCE_KIND_COMPOSITION, 1, 1, nullptr, 0, pDependencies, nDependencies };
	SIRResourceHandle hResult = {};
	REQUIRE( rWriter.publish( pub, &hResult ) == SIR_RESOURCE_RESULT_SUCCESS );
	return hResult;
}

static bool resourceTaskHasEdge( const SIRTaskGraph &rTasks, uint32_t nBefore, uint32_t nAfter ) noexcept
{
	for ( uint32_t i = 0; i < rTasks.getEdgeCount(); ++i ) {
		sirTaskGraphEdge_t edge = {};
		REQUIRE( rTasks.getEdge( i, &edge ) );
		if ( edge.nBefore == nBefore && edge.nAfter == nAfter ) {
			return true;
		}
	}
	return false;
}

static void initResourceTaskMemory( SIRMemoryManager &rMemory )
{
	const sirMemoryConfig_t config = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	REQUIRE( rMemory.init( config ) );
}

static void testCoreTaskGraphResourceDependencies( void )
{
	SIRTestAllocatorStats alloc = {};
	SIRMemoryManager memory;
	initResourceTaskMemory( memory );
	{
		SIRResourceGraph resources( testConfig( alloc, 16 ) );
		SIRResourceGraphWriter *pWriter = resources.getWriter();
		REQUIRE( pWriter != nullptr );
		sirResourceScheduledTestSet_t work = {};
		work.nNodes = 4;
		work.nodes[0].hResource = publishTaskNode( *pWriter, 801 );
		work.nodes[1].hResource = publishTaskNode( *pWriter, 802 );
		SIRResourceDependency edgesC[2] = { { key( 801 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                    { key( 802 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} } };
		work.nodes[2].hResource = publishTaskNode( *pWriter, 803, edgesC, 2 );
		SIRResourceDependency edgeD = { key( 803 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} };
		work.nodes[3].hResource = publishTaskNode( *pWriter, 804, &edgeD, 1 );
		work.nodes[2].pPreA = &work.nodes[0].nRuns;
		work.nodes[2].pPreB = &work.nodes[1].nRuns;
		work.nodes[3].pPreA = &work.nodes[2].nRuns;

		SIRTaskScheduler scheduler;
		REQUIRE( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRTaskGraph tasks;
		REQUIRE( tasks.init( &memory, { 8, 1, 16 } ) );
		SIRResourceDependencyPlanEntry plan[8] = {};
		uint32_t nCount = 0;
		REQUIRE( pWriter->compileTaskGraph( work.nodes[3].hResource, tasks, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nCount == 4 && tasks.getPassCount() == 4 && tasks.getEdgeCount() == 3 );
		REQUIRE( resourceTaskHasEdge( tasks, 0, 2 ) && resourceTaskHasEdge( tasks, 1, 2 ) && resourceTaskHasEdge( tasks, 2, 3 ) );
		REQUIRE( !resourceTaskHasEdge( tasks, 0, 1 ) );
		REQUIRE( pWriter->validateTaskPlan( plan, nCount ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( tasks.submit( &scheduler ) );
		REQUIRE( scheduler.pumpCPU( 8 ) == 4 );
		sirTaskGraphExecutionSummary_t summary = {};
		REQUIRE( tasks.queryExecution( &summary ) );
		REQUIRE( summary.eState == SIR_TASK_GRAPH_EXEC_SUCCEEDED && summary.nSucceeded == 4 && summary.nPending == 0 );
		for ( uint32_t i = 0; i < 4; ++i ) {
			REQUIRE( work.nodes[i].nRuns.load() == 1 );
		}
		REQUIRE( pWriter->validateTaskPlan( plan, nCount ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( tasks.releaseSubmitted() && tasks.reset() );
		REQUIRE( scheduler.shutdown() );
	}
	memory.shutdown();
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testCoreTaskGraphFailureAndCancellation( void )
{
	SIRTestAllocatorStats alloc = {};
	SIRMemoryManager memory;
	initResourceTaskMemory( memory );
	{
		SIRResourceGraph resources( testConfig( alloc, 8 ) );
		SIRResourceGraphWriter *pWriter = resources.getWriter();
		REQUIRE( pWriter != nullptr );
		sirResourceScheduledTestSet_t work = {};
		work.nNodes = 3;
		work.nodes[0].hResource = publishTaskNode( *pWriter, 901 );
		work.nodes[1].hResource = publishTaskNode( *pWriter, 902 );
		SIRResourceDependency dependencies[2] = { { key( 901 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                          { key( 902 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} } };
		work.nodes[2].hResource = publishTaskNode( *pWriter, 903, dependencies, 2 );
		work.nodes[2].pPreA = &work.nodes[0].nRuns;
		work.nodes[2].pPreB = &work.nodes[1].nRuns;
		work.nodes[0].bFail = true;
		SIRTaskScheduler scheduler;
		REQUIRE( scheduler.init( &memory, { 8, 0, 16 } ) );
		SIRTaskGraph tasks;
		REQUIRE( tasks.init( &memory, { 8, 1, 16 } ) );
		SIRResourceDependencyPlanEntry plan[8] = {};
		uint32_t nCount = 0;
		REQUIRE( pWriter->compileTaskGraph( work.nodes[2].hResource, tasks, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( tasks.submit( &scheduler ) );
		REQUIRE( scheduler.pumpCPU( 8 ) == 2 );
		sirTaskGraphExecutionSummary_t summary = {};
		REQUIRE( tasks.queryExecution( &summary ) );
		REQUIRE( summary.eState == SIR_TASK_GRAPH_EXEC_FAILED && summary.nFailed == 1 && summary.nCancelled == 1 );
		REQUIRE( work.nodes[2].nRuns.load() == 0 );
		REQUIRE( tasks.releaseSubmitted() && tasks.reset() );
		work.nodes[0].bFail = false;
		REQUIRE( pWriter->compileTaskGraph( work.nodes[2].hResource, tasks, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( tasks.submit( &scheduler ) );
		REQUIRE( tasks.cancelExecution() );
		REQUIRE( tasks.queryExecution( &summary ) && summary.eState == SIR_TASK_GRAPH_EXEC_CANCELLED );
		REQUIRE( tasks.releaseSubmitted() );
		REQUIRE( scheduler.shutdown() );
	}
	memory.shutdown();
	REQUIRE( alloc.m_nOutstanding.load() == 0 );
}

static void testCoreTaskGraphStaleRevisionsAndBudget( void )
{
	SIRTestAllocatorStats alloc = {};
	SIRMemoryManager memory;
	initResourceTaskMemory( memory );
	{
		SIRResourceGraph resources( testConfig( alloc, 8 ) );
		SIRResourceGraphWriter *pWriter = resources.getWriter();
		REQUIRE( pWriter != nullptr );
		sirResourceScheduledTestSet_t work = {};
		work.nNodes = 3;
		work.nodes[0].hResource = publishTaskNode( *pWriter, 951 );
		work.nodes[1].hResource = publishTaskNode( *pWriter, 952 );
		SIRResourceDependency dependencies[2] = { { key( 951 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} },
		                                          { key( 952 ), SIR_RESOURCE_EDGE_KIND_REQUIRED, {} } };
		work.nodes[2].hResource = publishTaskNode( *pWriter, 953, dependencies, 2 );

		SIRTaskGraph underBudget;
		REQUIRE( underBudget.init( &memory, { 8, 1, 1 } ) );
		SIRResourceDependencyPlanEntry plan[8] = {};
		uint32_t nCount = 0;
		REQUIRE( pWriter->compileTaskGraph( work.nodes[2].hResource, underBudget, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
		REQUIRE( underBudget.getPassCount() == 0 );
		SIRTaskGraph tasks;
		REQUIRE( tasks.init( &memory, { 8, 1, 16 } ) );
		std::atomic<uint32_t> eOther{ 0 };
		std::thread other( [&]() {
			uint32_t nOtherCount = 0;
			eOther.store( pWriter->compileTaskGraph( work.nodes[2].hResource, tasks, plan, 8, describeResourceTest, &work, &nOtherCount ) );
		} );
		other.join();
		REQUIRE( eOther.load() == SIR_RESOURCE_RESULT_WRITER_VIOLATION );
		REQUIRE( pWriter->compileTaskGraph( work.nodes[2].hResource, tasks, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( nCount == 3 );
		const SIRResourceDependencyPlanEntry rootPlan = plan[2];
		REQUIRE( rootPlan.nInvalidationSequence == 0 );
		publishTaskNode( *pWriter, 951 );
		REQUIRE( pWriter->validateTaskPlan( &rootPlan, 1 ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		REQUIRE( pWriter->validateTaskPlan( plan, nCount ) == SIR_RESOURCE_RESULT_STALE_REVISION );
		REQUIRE( tasks.reset() );
		work.bRejectFactory = true;
		REQUIRE( pWriter->compileTaskGraph( work.nodes[2].hResource, tasks, plan, 8, describeResourceTest, &work, &nCount ) ==
		         SIR_RESOURCE_RESULT_FAILED );
		REQUIRE( tasks.getPassCount() == 0 );
	}
	memory.shutdown();
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
	testExpectedRevisionPublication();
	testReverseDependenciesAndTransitiveInvalidation();
	testReverseDependencyCapacityRollback();
	testInvalidationJournalOverflowAndResume();
	testAtomicReadIndexCollisionsAndChurn();
	testEpochReclamationWithOverlappingScopes();
	testReaderEpochSlotExhaustion();
	testDeterministicDependencyOrderCompilation();
	testCoreTaskGraphResourceDependencies();
	testCoreTaskGraphFailureAndCancellation();
	testCoreTaskGraphStaleRevisionsAndBudget();
	printf( "PASS: GRDG resource registry, publication, deduplication, lifetime, concurrency\n" );
	return 0;
}

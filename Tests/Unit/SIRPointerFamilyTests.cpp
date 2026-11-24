
#include <Resources/SIRResourceGraph.h>
#include <Resources/SIRResourcePtr.h>
#include <Resources/SIRWeakResourcePtr.h>
#include <Resources/SIRResourceRevisionPtr.h>
#include <Resources/SIRSoftResourcePtr.h>
#include <Resources/SIRDerivedResourcePtr.h>
#include <Core/Memory/Pointers/SIRUniquePtr.h>
#include <Core/Memory/Pointers/SIRIntrusivePtr.h>
#include <Core/Memory/Pointers/SIRObserverPtr.h>
#include <Core/Memory/Pointers/SIROffsetPtr.h>
#include <Core/Memory/Pointers/SIRHazardPtr.h>
#include <Core/Memory/Pointers/SIRRCUPtr.h>
#include <Core/Memory/Pointers/SIRDeferredPtr.h>
#include <Core/Memory/Pointers/SIRFenceRetiredPtr.h>
#include <Core/Platform/SIRThread.h>
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
	std::atomic<uint32_t> m_nFailOnAllocation;
	std::atomic<uint32_t> m_nAllocationCalls;
};

static void *testAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	SIRTestAllocatorStats *pStats = ( SIRTestAllocatorStats * )pContext;
	if ( pStats->m_bFailNext.exchange( false ) ) {
		return nullptr;
	}
	const uint32_t nFail = pStats->m_nFailOnAllocation.load( std::memory_order_relaxed );
	if ( nFail != 0 && pStats->m_nAllocationCalls.fetch_add( 1, std::memory_order_relaxed ) + 1 == nFail ) {
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

static SIRResourceGraphConfig testConfig( SIRTestAllocatorStats& rStats, uint32_t nSlots = 64, uint64_t nBudget = 4 * 1024 * 1024 ) noexcept
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

static SIRResourceHandle publishText( SIRResourceGraphWriter& rWriter, SIRResourceKey resourceKey, const char *szData, uint64_t nHash,
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

static void testGRDGReferences( void )
{
	SIRTestAllocatorStats allocator = {};
	{
		SIRResourceGraph graph( testConfig( allocator, 2 ) );
		REQUIRE( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		REQUIRE( pWriter != nullptr );
		const SIRResourceKey resourceKey = key( 801 );
		const SIRResourceHandle hResource = publishText( *pWriter, resourceKey, "abc", 801 );
		SIRResourcePtr<SIRBlobResourceView> strong;
		REQUIRE( SIRResourcePtr<SIRBlobResourceView>::acquire( graph, hResource, &strong ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( graph.acquireReference( hResource, SIR_RESOURCE_KIND_IMAGE ) == SIR_RESOURCE_RESULT_TYPE_MISMATCH );
		REQUIRE( pWriter->retire( hResource ) == SIR_RESOURCE_RESULT_BUSY );
		SIRResourcePtr<SIRBlobResourceView> copy( strong );
		SIRWeakResourcePtr<SIRBlobResourceView> weak( strong );
		REQUIRE( !weak.isExpired() );
		strong.reset();
		REQUIRE( pWriter->retire( hResource ) == SIR_RESOURCE_RESULT_BUSY );
		REQUIRE( weak.lock( &strong ) == SIR_RESOURCE_RESULT_SUCCESS );
		copy.reset();
		strong.reset();

		SIRSoftResourcePtr<SIRBlobResourceView> soft( resourceKey );
		SIRResourceRevisionPtr<SIRBlobResourceView> revision;
		{
			SIRResourceReadScope scope( graph );
			REQUIRE( scope.isActive() );
			SIRResourceHandle hFound = {};
			REQUIRE( soft.find( graph, scope, &hFound ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( hFound.m_nGeneration == hResource.m_nGeneration );
			SIRResourceReadPtr<SIRBlobResourceView> read;
			REQUIRE( soft.read( graph, scope, &read ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( read.getRevision() == 1 );
			SIRResourceView baseBefore;
			REQUIRE( graph.resolve( scope, hResource, &baseBefore ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRBlobResourceView typed( baseBefore );
			REQUIRE( read.get( &typed ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( typed.getRevision() == 1 );
			SIRResourceView base;
			REQUIRE( graph.resolve( scope, hResource, &base ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( revision.pin( graph, scope, base, 91 ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( revision.getRevision() == 1 );
			REQUIRE( soft.lock( graph, scope, &strong ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( publishText( *pWriter, resourceKey, "def", 802 ).m_nGeneration == hResource.m_nGeneration );
		REQUIRE( revision.getRevision() == 1 );
		REQUIRE( revision.reset() == SIR_RESOURCE_RESULT_SUCCESS );
		strong.reset();
		REQUIRE( pWriter->retire( hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( weak.isExpired() );
		REQUIRE( weak.lock( &strong ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( graph.acquireReference( hResource, SIR_RESOURCE_KIND_BLOB ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		REQUIRE( pWriter->collectRetired( 128 ) > 0 );
	}
	REQUIRE( allocator.m_nOutstanding.load() == 0 );
}

static void testDeferredRetirement( void )
{
	uint32_t nDestroyed = 0;
	sirDeferredRetirementEntry_t slots[2] = {};
	SIRDeferredRetirementQueue queue( slots, 2 );
	{
		SIRDeferredPtr<int> deferred( new int( 14 ), queue, &nDestroyed, []( void *pContext, void *pObject ) noexcept {
			++*( uint32_t * )pContext;
			delete ( int * )pObject;
		} );
		REQUIRE( *deferred.get() == 14 );
	}
	REQUIRE( nDestroyed == 0 );
	REQUIRE( queue.pending() == 1 );
	REQUIRE( queue.drain( 1 ) == 1 );
	REQUIRE( nDestroyed == 1 );
}

static void testFenceRetirement( void )
{
	uint32_t nDestroyed = 0;
	sirFenceRetiredEntry_t entries[2] = {};
	SIRFenceRetirementQueue queue( entries, 2 );
	const sirProviderCompletionSet_t required = { 4, 2, 1, 1, { 9, 0, 0, 0 } };
	{
		sirFenceRetirementPolicy_t policy = { &nDestroyed,
		                                      []( void *pContext, void *pObject ) noexcept {
			                                      ++*( uint32_t * )pContext;
			                                      delete ( int * )pObject;
		                                      },
		                                      required };
		SIRFenceRetiredPtr<int> owned( new int( 42 ), queue, policy );
	}
	const sirProviderCompletionSet_t wrongGeneration = { 4, 1, 1, 1, { 99, 0, 0, 0 } };
	const sirProviderCompletionSet_t incomplete = { 4, 2, 1, 1, { 8, 0, 0, 0 } };
	const sirProviderCompletionSet_t completed = { 4, 2, 1, 1, { 10, 0, 0, 0 } };
	REQUIRE( queue.collect( wrongGeneration, 2 ) == 0 );
	REQUIRE( queue.collect( incomplete, 2 ) == 0 );
	REQUIRE( nDestroyed == 0 );
	REQUIRE( queue.collect( completed, 2 ) == 1 );
	REQUIRE( nDestroyed == 1 );
}

static void testHazardRetirement( void )
{
	SIRHazardRecord records[2];
	SIRHazardDomain domain( records, 2 );
	int *pInitial = new int( 12 );
	std::atomic<int *> source( pInitial );
	uint32_t nDestroyed = 0;
	sirHazardRetiredEntry_t entries[2] = {};
	SIRHazardRetireList retired( domain, entries, 2 );
	SIRHazardPtr<int> reader;
	REQUIRE( reader.protect( domain, source ) );
	REQUIRE( reader.get() == pInitial );
	REQUIRE( source.exchange( nullptr ) == pInitial );
	const sirHazardRetiredEntry_t entry = { pInitial, &nDestroyed, []( void *pContext, void *pObject ) noexcept {
		                                       ++*( uint32_t * )pContext;
		                                       delete ( int * )pObject;
	                                       } };
	REQUIRE( retired.retire( entry ) );
	REQUIRE( retired.collect() == 0 );
	reader.reset();
	REQUIRE( retired.collect() == 1 );
	REQUIRE( nDestroyed == 1 );
}

static void testRCUEpoch( void )
{
	SIRRCUReaderRecord records[2];
	SIRRCUDomain domain( records, 2 );
	int *pOld = new int( 11 );
	int *pNew = new int( 22 );
	std::atomic<int *> source( pOld );
	SIRRCUPtr<int> reader;
	REQUIRE( reader.read( domain, source ) );
	REQUIRE( *reader.get() == 11 );
	const sirRCURetiredEntry_t previous = domain.publish( source, pNew );
	REQUIRE( previous.pObject == pOld );
	REQUIRE( !domain.canReclaim( previous.nEpoch ) );
	uint32_t nDestroyed = 0;
	sirRCURetirementEntry_t entries[1] = {};
	SIRRCURetireList list( domain, entries, 1 );
	REQUIRE( list.retire( { previous, &nDestroyed, []( void *pContext, void *pObject ) noexcept {
		                       ++*( uint32_t * )pContext;
		                       delete ( int * )pObject;
	                       } } ) );
	REQUIRE( list.collect() == 0 );
	reader.reset();
	REQUIRE( list.collect() == 1 );
	REQUIRE( nDestroyed == 1 );
	REQUIRE( reader.read( domain, source ) );
	REQUIRE( *reader.get() == 22 );
	reader.reset();
	delete source.exchange( nullptr );
}

static void testUniqueAndIntrusive( void )
{
	struct sirCustomDeleter_s
	{
		uint32_t *pCount;
		void operator()( int *pObject ) const noexcept
		{
			++*pCount;
			delete pObject;
		}
	};
	uint32_t nDestroyed = 0;
	{
		SIRUniquePtr<int, sirCustomDeleter_s> owner( new int( 50 ), { &nDestroyed } );
		REQUIRE( *owner == 50 );
	}
	REQUIRE( nDestroyed == 1 );
}

typedef struct sirPointerTestProvider_s
{
	uint32_t nReleased;
	uint64_t nToken;
} sirPointerTestProvider_t;

static bool releasePointerTestNative( void *pContext, uint64_t nToken ) noexcept
{
	sirPointerTestProvider_t *pProvider = ( sirPointerTestProvider_t * )pContext;
	++pProvider->nReleased;
	pProvider->nToken = nToken;
	return true;
}

static void testDerivedRetirement( void )
{
	SIRTestAllocatorStats allocator = {};
	{
		SIRResourceGraph graph( testConfig( allocator ) );
		REQUIRE( graph.isReady() );
		const SIRResourceHandle hSource = publishText( *graph.getWriter(), key( 900 ), "data", 900 );
		SIRResourceDerivedConfig config = {};
		config.allocator = { &allocator, testAllocate, testFree };
		config.nMetadataDomain = 7;
		config.nMaxRepresentations = 4;
		config.nMaxProviders = 1;
		config.nMaxTrackedBytes = 2048;
		SIRResourceDerivedRegistry registry( graph, config );
		REQUIRE( registry.isReady() );
		sirPointerTestProvider_t provider = {};
		REQUIRE( registry.registerProvider( 7, { &provider, releasePointerTestNative } ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRDerivedResourceHandle hDerived = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hSource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRDerivedResourceRegistration registration = {};
			registration.m_Key = { hSource, 1, 7, 1, 0, 0 };
			registration.m_Accounting = { SIR_MEMORY_SPACE_EXTERNAL, 5, 64, 64, 1 };
			registration.m_nNativeToken = 33;
			REQUIRE( registry.registerRepresentation( scope, view, registration, &hDerived ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		const sirProviderCompletionSet_t required = { 7, 1, 1, 1, { 18, 0, 0, 0 } };
		{
			SIRDerivedResourcePtr derived;
			REQUIRE( derived.adopt( registry, hDerived, required ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( derived.isValid() );
		}
		REQUIRE( provider.nReleased == 0 );
		REQUIRE( registry.updateProviderCompletion( { 7, 1, 1, 1, { 17, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 0 );
		REQUIRE( registry.updateProviderCompletion( required ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( provider.nReleased == 1 );
		REQUIRE( provider.nToken == 33 );
		REQUIRE( graph.getWriter()->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		( void )graph.getWriter()->collectRetired( 128 );
	}
	REQUIRE( allocator.m_nOutstanding.load() == 0 );
}

typedef struct sirPointerWeakRace_s
{
	SIRWeakResourcePtr<SIRBlobResourceView> *pWeak;
	std::atomic<uint32_t> nInvalid;
} sirPointerWeakRace_t;

static void weakRaceWorker( void *pContext )
{
	sirPointerWeakRace_t *pState = ( sirPointerWeakRace_t * )pContext;
	for ( uint32_t iRound = 0; iRound < 15000; ++iRound ) {
		SIRResourcePtr<SIRBlobResourceView> acquired;
		const SIRResourceResult eResult = pState->pWeak->lock( &acquired );
		if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
			acquired.reset();
		} else if ( eResult != SIR_RESOURCE_RESULT_INVALID_HANDLE ) {
			pState->nInvalid.fetch_add( 1 );
		}
	}
}

static void testConcurrentResourceLockAndRetire( void )
{
	SIRTestAllocatorStats allocator = {};
	{
		SIRResourceGraph graph( testConfig( allocator, 1 ) );
		REQUIRE( graph.isReady() );
		const SIRResourceHandle hResource = publishText( *graph.getWriter(), key( 990 ), "entry", 990 );
		SIRResourcePtr<SIRBlobResourceView> strong;
		REQUIRE( SIRResourcePtr<SIRBlobResourceView>::acquire( graph, hResource, &strong ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRWeakResourcePtr<SIRBlobResourceView> weak( strong );
		sirPointerWeakRace_t state = { &weak, std::atomic<uint32_t>( 0 ) };
		SIRThread worker;
		REQUIRE( worker.start( weakRaceWorker, &state ) );
		strong.reset();
		bool bRetired = false;
		for ( uint32_t iTry = 0; iTry < 15000; ++iTry ) {
			const SIRResourceResult eResult = graph.getWriter()->retire( hResource );
			if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
				bRetired = true;
				break;
			}
			REQUIRE( eResult == SIR_RESOURCE_RESULT_BUSY );
		}
		REQUIRE( worker.join() );
		REQUIRE( state.nInvalid.load() == 0 );
		if ( !bRetired ) {
			REQUIRE( graph.getWriter()->retire( hResource ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		REQUIRE( weak.isExpired() );
		( void )graph.getWriter()->collectRetired( 128 );
	}
	REQUIRE( allocator.m_nOutstanding.load() == 0 );
}

typedef struct sirPointerHazardRace_s
{
	SIRHazardDomain *pDomain;
	std::atomic<int *> *pSource;
	std::atomic<uint32_t> nInvalid;
} sirPointerHazardRace_t;

static void hazardRaceWorker( void *pContext )
{
	sirPointerHazardRace_t *pState = ( sirPointerHazardRace_t * )pContext;
	SIRHazardPtr<int> guard;
	for ( uint32_t iRound = 0; iRound < 20000; ++iRound ) {
		if ( guard.protect( *pState->pDomain, *pState->pSource ) ) {
			int *pValue = guard.get();
			if ( pValue != nullptr && *pValue != 11 && *pValue != 22 ) {
				pState->nInvalid.fetch_add( 1 );
			}
			guard.reset();
		}
	}
}

static void testConcurrentHazardAccess( void )
{
	SIRHazardRecord records[2];
	SIRHazardDomain domain( records, 2 );
	int *pInitial = new int( 11 );
	int *pReplacement = new int( 22 );
	std::atomic<int *> source( pInitial );
	uint32_t nDestroyed = 0;
	sirHazardRetiredEntry_t entries[2] = {};
	SIRHazardRetireList retired( domain, entries, 2 );
	sirPointerHazardRace_t state = { &domain, &source, std::atomic<uint32_t>( 0 ) };
	SIRThread worker;
	REQUIRE( worker.start( hazardRaceWorker, &state ) );
	int *pPrior = source.exchange( pReplacement );
	REQUIRE( retired.retire( { pPrior, &nDestroyed, []( void *pContext, void *pObject ) noexcept {
		                          ++*( uint32_t * )pContext;
		                          delete ( int * )pObject;
	                          } } ) );
	( void )retired.collect();
	REQUIRE( worker.join() );
	REQUIRE( state.nInvalid.load() == 0 );
	( void )retired.collect();
	REQUIRE( nDestroyed == 1 );
	delete source.exchange( nullptr );
}

typedef struct sirPointerRCURace_s
{
	SIRRCUDomain *pDomain;
	std::atomic<int *> *pSource;
	std::atomic<uint32_t> nInvalid;
} sirPointerRCURace_t;

static void rcuRaceWorker( void *pContext )
{
	sirPointerRCURace_t *pState = ( sirPointerRCURace_t * )pContext;
	SIRRCUPtr<int> guard;
	for ( uint32_t iRound = 0; iRound < 20000; ++iRound ) {
		if ( guard.read( *pState->pDomain, *pState->pSource ) ) {
			const int *pValue = guard.get();
			if ( pValue == nullptr || *pValue < 0 || *pValue > 300 ) {
				pState->nInvalid.fetch_add( 1 );
			}
			guard.reset();
		}
	}
}

static void testConcurrentRCU( void )
{
	SIRRCUReaderRecord records[2];
	SIRRCUDomain domain( records, 2 );
	int nValues[301] = {};
	for ( int iIndex = 0; iIndex < 301; ++iIndex ) {
		nValues[iIndex] = iIndex;
	}
	std::atomic<int *> source( &nValues[0] );
	sirPointerRCURace_t state = { &domain, &source, std::atomic<uint32_t>( 0 ) };
	SIRThread worker;
	REQUIRE( worker.start( rcuRaceWorker, &state ) );
	sirRCURetiredEntry_t last = {};
	for ( uint32_t iIndex = 1; iIndex <= 300; ++iIndex ) {
		last = domain.publish( source, &nValues[iIndex] );
	}
	REQUIRE( worker.join() );
	REQUIRE( state.nInvalid.load() == 0 );
	REQUIRE( domain.canReclaim( last.nEpoch ) );
	REQUIRE( source.load() == &nValues[300] );
}

typedef struct sirPointerIntrusiveFixture_s
{
	uint32_t nReferences;
	uint32_t *pDestroyCount;
} sirPointerIntrusiveFixture_t;

void SIRIntrusiveAddRef( sirPointerIntrusiveFixture_t *pObject ) noexcept
{
	++pObject->nReferences;
}

void SIRIntrusiveRelease( sirPointerIntrusiveFixture_t *pObject ) noexcept
{
	REQUIRE( pObject->nReferences > 0 );
	--pObject->nReferences;
	if ( pObject->nReferences == 0 ) {
		++*pObject->pDestroyCount;
		delete pObject;
	}
}

static void testImportedPointerWrappers( void )
{
	uint32_t nDestroyed = 0;
	{
		SIRIntrusivePtr<sirPointerIntrusiveFixture_t> first( new sirPointerIntrusiveFixture_t{ 0, &nDestroyed } );
		REQUIRE( first.isValid() );
		REQUIRE( first->nReferences == 1 );
		SIRIntrusivePtr<sirPointerIntrusiveFixture_t> second( first );
		REQUIRE( second->nReferences == 2 );
	}
	REQUIRE( nDestroyed == 1 );

	typedef struct sirOffsetFixture_s
	{
		uint32_t nPadding;
		uint32_t nData;
	} sirOffsetFixture_t;
	const sirOffsetFixture_t blob = { 9, 33 };
	SIROffsetPtr<uint32_t> offset( sizeof( uint32_t ) );
	REQUIRE( offset.resolve( &blob, sizeof( blob ) ) == &blob.nData );
	REQUIRE( offset.resolve( &blob, sizeof( uint32_t ) ) == nullptr );
	SIRObserverPtr<const uint32_t> observer( &blob.nData );
	REQUIRE( observer.isValid() );
	REQUIRE( *observer == 33 );
	observer.reset();
	REQUIRE( !observer.isValid() );
}

int main( void )
{
	testGRDGReferences();
	testDeferredRetirement();
	testFenceRetirement();
	testHazardRetirement();
	testRCUEpoch();
	testUniqueAndIntrusive();
	testDerivedRetirement();
	testConcurrentResourceLockAndRetire();
	testConcurrentHazardAccess();
	testConcurrentRCU();
	testImportedPointerWrappers();
	return 0;
}

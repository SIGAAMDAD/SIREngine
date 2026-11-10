/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Resources/SIRResourceGraph.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#endif

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

struct sirSharedTestAllocator_t
{
	uint32_t nLive = 0;
	uint32_t nAllocationAttempts = 0;
	bool bSealed = false;
};

static void *testAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t ) noexcept
{
	sirSharedTestAllocator_t *pState = ( sirSharedTestAllocator_t * )pContext;
	++pState->nAllocationAttempts;
	if ( pState->bSealed || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pMemory = nullptr;
#ifdef _WIN32
	pMemory = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pMemory, nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pMemory ) {
		++pState->nLive;
	}
	return pMemory;
}

static void testFree( void *pContext, void *pMemory, uint64_t, uint32_t, uint32_t ) noexcept
{
	--( ( sirSharedTestAllocator_t * )pContext )->nLive;
#ifdef _WIN32
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

struct sirSharedNative_t
{
	uint32_t nReleases = 0;
	uint64_t nLastToken = 0;
};

static bool nativeRelease( void *pContext, uint64_t nToken ) noexcept
{
	sirSharedNative_t *pNative = ( sirSharedNative_t * )pContext;
	++pNative->nReleases;
	pNative->nLastToken = nToken;
	return true;
}

static SIRResourceHandle publish( SIRResourceGraphWriter& rWriter, uint64_t nAssetId, uint64_t nFingerprint ) noexcept
{
	SIRResourceStagingHandle staging = {};
	SIRResourceMutableBytes bytes = {};
	CHECK( rWriter.allocateStaging( 64, 16, 1, &staging, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
	memset( bytes.m_pData, 0xA5, 64 );
	const SIRResourceChunkPublication chunk = { { 1 }, { nFingerprint, nFingerprint ^ UINT64_C( 0xac3f875124e6 ), 8, 0 }, staging, false };
	const SIRResourcePublication publication = { { nAssetId, 18, 0 }, SIR_RESOURCE_KIND_GEOMETRY, 1, 8, &chunk, 1, nullptr, 0 };
	SIRResourceHandle handle = {};
	CHECK( rWriter.publish( publication, &handle ) == SIR_RESOURCE_RESULT_SUCCESS );
	return handle;
}

static void testSharedBackingLeases( void )
{
	sirSharedTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig graphConfig = {};
		graphConfig.m_Allocator = { &allocator, testAllocate, testFree };
		graphConfig.m_nMetadataDomain = 1;
		graphConfig.m_nMaxResources = 8;
		graphConfig.m_nMaxStaging = 8;
		graphConfig.m_nMaxProviderPins = 16;
		graphConfig.m_nMaxCommittedBytes = 1024 * 1024;
		SIRResourceGraph graph( graphConfig );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter );
		const SIRResourceHandle hSource = publish( *pWriter, 201, 9182 );
		const SIRResourceHandle hAlias = publish( *pWriter, 202, 9182 );
		CHECK( graph.getStats().m_nUniqueChunks == 1 );

		SIRResourceDerivedConfig config = {};
		config.allocator = graphConfig.m_Allocator;
		config.nMetadataDomain = 1;
		config.nMaxProviders = 3;
		config.nMaxRepresentations = 5;
		config.nMaxSharedBorrows = 8;
		config.nMaxTrackedBytes = 300;
		SIRResourceDerivedRegistry registry( graph, config );
		CHECK( registry.isReady() );
		sirSharedNative_t driver = {}, physics = {}, audio = {};
		CHECK( registry.registerProvider( 7, 3, 2, { &driver, nativeRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.registerProvider( 8, 3, 2, { &physics, nativeRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.registerProvider( 9, 3, 2, { &audio, nativeRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );

		const sirSharedDeviceBackingDesc_t desc = { 44, 3, 11, 8, 0xF, 16, 1, 7, 128, 112 };
		SIRDerivedResourceHandle hBacking = {};
		sirSharedDeviceView_t views[4] = {};
		uint64_t nOriginalBackingId = 0;
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView source, alias;
			CHECK( graph.resolve( scope, hSource, &source ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( graph.resolve( scope, hAlias, &alias ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceBackingReservation_t reservation = {};
			CHECK( registry.reserveSharedDeviceBacking( scope, source, desc, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( reservation.bNeedsNativeCreation );
			hBacking = reservation.hBacking;
			nOriginalBackingId = reservation.nPhysicalBackingId;
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, desc, &reservation ) == SIR_RESOURCE_RESULT_BUSY );
			CHECK( !reservation.bNeedsNativeCreation && reservation.nPhysicalBackingId == nOriginalBackingId );
			CHECK( registry.publishSharedDeviceBacking( hBacking, 9001 ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceBackingDesc_t staleProvider = desc;
			staleProvider.nDeviceGeneration = 4;
			CHECK( registry.reserveSharedDeviceBacking( scope, source, staleProvider, &reservation ) ==
			       SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			CHECK( registry.publishSharedDeviceBacking( hBacking, 9002 ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, desc, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( !reservation.bNeedsNativeCreation && reservation.nPhysicalBackingId == nOriginalBackingId );
			sirSharedDeviceBackingDesc_t incompatible = desc;
			incompatible.nStorageLayoutId = 12;
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, incompatible, &reservation ) == SIR_RESOURCE_RESULT_INVALID_FORMAT );
			incompatible = desc;
			incompatible.nSupportedUsageMask = 0x10;
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, incompatible, &reservation ) == SIR_RESOURCE_RESULT_INVALID_FORMAT );
			incompatible = desc;
			incompatible.nAlignment = 32;
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, incompatible, &reservation ) == SIR_RESOURCE_RESULT_INVALID_FORMAT );
			sirSharedDeviceBackingDesc_t oversized = desc;
			oversized.nPhysicalDeviceId = 55;
			oversized.nCommittedBytes = 301;
			CHECK( registry.reserveSharedDeviceBacking( scope, source, oversized, &reservation ) == SIR_RESOURCE_RESULT_OUT_OF_BUDGET );
			sirSharedDeviceBackingDesc_t canceled = desc;
			canceled.nPhysicalDeviceId = 55;
			CHECK( registry.reserveSharedDeviceBacking( scope, source, canceled, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( reservation.bNeedsNativeCreation );
			CHECK( registry.retireRepresentation( reservation.hBacking, { 7, 3, 1, 2, { 1, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_BUSY );
			CHECK( registry.cancelSharedDeviceBacking( reservation.hBacking ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( registry.getStats( nullptr ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
			SIRResourceDerivedStats stats = {};
			CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 128 && stats.nLiveRepresentations == 1 );
			const uint64_t nEpoch = graph.getStats().nPublicationEpoch;
			const uint32_t nAllocationsBeforeSealing = allocator.nAllocationAttempts;
			allocator.bSealed = true;
			CHECK( registry.reserveSharedDeviceBacking( scope, alias, desc, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( !reservation.bNeedsNativeCreation && allocator.nAllocationAttempts == nAllocationsBeforeSealing );
			for ( uint32_t i = 0; i < 4; ++i ) {
				sirSharedDeviceViewRequest_t request = {
				    44, 1, nEpoch, ( uint64_t )i * 16, 16, 3, 11, 8, 1, 16, i + 1, i == 0 ? 7u : ( i == 1 ? 8u : 9u ) };
				CHECK( registry.acquireSharedDeviceView( scope, i == 2 ? alias : source, request, &views[i] ) ==
				       SIR_RESOURCE_RESULT_SUCCESS );
				CHECK( views[i].nPhysicalBackingId == nOriginalBackingId && views[i].nNativeResourceToken == 9001 );
				CHECK( views[i].bReadOnly && views[i].nApprovedUsageMask == 1 && views[i].nDeviceGeneration == 3 &&
				       views[i].nSourceRevision == 1 );
				request.nExpectedPublicationEpoch = 0;
				CHECK( registry.acquireSharedDeviceView( scope, source, request, &views[i] ) == SIR_RESOURCE_RESULT_INVALID_ARGUMENT );
				request.nExpectedPublicationEpoch = nEpoch;
				request.nBytes = 256;
				CHECK( registry.acquireSharedDeviceView( scope, source, request, &views[i] ) == SIR_RESOURCE_RESULT_INVALID_FORMAT );
				request.nBytes = 16;
				request.nExpectedPublicationEpoch = nEpoch - 1;
				CHECK( registry.acquireSharedDeviceView( scope, source, request, &views[i] ) == SIR_RESOURCE_RESULT_STALE_REVISION );
				request.nExpectedPublicationEpoch = nEpoch;
				request.nExpectedSourceRevision = 2;
				CHECK( registry.acquireSharedDeviceView( scope, source, request, &views[i] ) == SIR_RESOURCE_RESULT_STALE_REVISION );
			}
			CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 128 && stats.nLiveSharedBorrows == 4 );
			CHECK( allocator.nAllocationAttempts == nAllocationsBeforeSealing );
			allocator.bSealed = false;
		}
		// A forged/modified borrowed view must not release somebody else's
		// canonical source pin, even if the borrow-slot generation is valid.
		const sirProviderCompletionSet_t releaseFence = { 7, 3, 3, 2, { 4, 9, 0, 0 } };
		sirSharedDeviceView_t forged = views[0];
		forged.nNativeResourceToken = 12345;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nOffset = 16;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nBytes = 8;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nSourceRevision = 999;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nDeviceGeneration = 9;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nApprovedUsageMask = 0xF;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.bReadOnly = false;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		forged = views[0];
		forged.nCompletionProviderId = 999;
		CHECK( registry.releaseSharedDeviceView( forged, releaseFence ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		SIRResourceDerivedStats beforeValidRelease = {};
		CHECK( registry.getStats( &beforeValidRelease ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( beforeValidRelease.nLiveSharedBorrows == 4 && driver.nReleases == 0 );
		CHECK( registry.retireRepresentation( hBacking, { 7, 3, 3, 2, { 4, 9, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( views[0], { 7, 3, 3, 2, { 4, 9, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( views[1], { 8, 3, 1, 2, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( views[2], { 9, 3, 2, 2, { 0, 7, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( views[3], { 9, 3, 1, 2, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( views[3], { 9, 3, 1, 2, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		CHECK( registry.updateProviderCompletion( { 7, 3, 1, 2, { 4, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.updateProviderCompletion( { 7, 3, 2, 2, { 0, 9, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( driver.nReleases == 0 );
		CHECK( registry.updateProviderCompletion( { 8, 3, 1, 2, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( driver.nReleases == 0 );
		CHECK( registry.updateProviderCompletion( { 9, 3, 1, 2, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( driver.nReleases == 0 );
		CHECK( registry.updateProviderCompletion( { 9, 3, 2, 2, { 0, 7, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( driver.nReleases == 1 && driver.nLastToken == 9001 && physics.nReleases == 0 && audio.nReleases == 0 );
		SIRResourceDerivedStats final = {};
		CHECK( registry.getStats( &final ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( final.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 0 && final.nLiveSharedBorrows == 0 );
		CHECK( registry.cancelSharedDeviceBacking( hBacking ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		CHECK( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( pWriter->retire( hAlias ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( pWriter->collectRetired( 8 ) == 2 );
	}
	CHECK( allocator.nLive == 0 );
}

static void testRevisionAndNativeToken( void )
{
	sirSharedTestAllocator_t allocator = {};
	{
		SIRResourceGraphConfig config = {};
		config.m_Allocator = { &allocator, testAllocate, testFree };
		config.m_nMetadataDomain = 1;
		config.m_nMaxResources = 8;
		config.m_nMaxStaging = 8;
		config.m_nMaxProviderPins = 8;
		config.m_nMaxCommittedBytes = 1024 * 1024;
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter );
		const SIRResourceHandle hOld = publish( *pWriter, 301, 9190 );
		SIRResourceDerivedConfig registryConfig = {};
		registryConfig.allocator = config.m_Allocator;
		registryConfig.nMetadataDomain = 1;
		registryConfig.nMaxProviders = 2;
		registryConfig.nMaxRepresentations = 3;
		registryConfig.nMaxSharedBorrows = 3;
		registryConfig.nMaxTrackedBytes = 512;
		SIRResourceDerivedRegistry registry( graph, registryConfig );
		CHECK( registry.isReady() );
		sirSharedNative_t native = {};
		CHECK( registry.registerProvider( 11, 4, 2, { &native, nativeRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );
		sirSharedNative_t replacementDevice = {};
		CHECK( registry.registerProvider( 12, 5, 2, { &replacementDevice, nativeRelease } ) == SIR_RESOURCE_RESULT_SUCCESS );
		const sirSharedDeviceBackingDesc_t desc = { 90, 4, 3, 8, 0xF, 16, 1, 11, 128, 128 };
		SIRDerivedResourceHandle hFirst = {}, hSecond = {};
		sirSharedDeviceView_t older = {}, newer = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView old;
			CHECK( graph.resolve( scope, hOld, &old ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceBackingReservation_t reservation = {};
			CHECK( registry.reserveSharedDeviceBacking( scope, old, desc, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			hFirst = reservation.hBacking;
			CHECK( registry.publishSharedDeviceBacking( hFirst, 501 ) == SIR_RESOURCE_RESULT_SUCCESS );
			// A second backend generation must drain the old physical allocation
			// before creating a replacement, even under a different provider ID.
			sirSharedDeviceBackingDesc_t switched = desc;
			switched.nDeviceGeneration = 5;
			switched.nProviderId = 12;
			const uint32_t nNoNewAllocations = allocator.nAllocationAttempts;
			CHECK( registry.reserveSharedDeviceBacking( scope, old, switched, &reservation ) == SIR_RESOURCE_RESULT_BUSY );
			CHECK( allocator.nAllocationAttempts == nNoNewAllocations );
			SIRResourceDerivedStats beforeSwitch = {};
			CHECK( registry.getStats( &beforeSwitch ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( beforeSwitch.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 128 && beforeSwitch.nLiveRepresentations == 1 );
			const sirSharedDeviceViewRequest_t request = { 90, 1, graph.getStats().nPublicationEpoch, 0, 64, 4, 3, 8, 1, 16, 1, 11 };
			CHECK( registry.acquireSharedDeviceView( scope, old, request, &older ) == SIR_RESOURCE_RESULT_SUCCESS );
		}
		const SIRResourceHandle hNew = publish( *pWriter, 301, 9190 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView updated;
			CHECK( graph.resolve( scope, hNew, &updated ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( updated.getRevision() == 2 );
			sirSharedDeviceBackingReservation_t reservation = {};
			CHECK( registry.reserveSharedDeviceBacking( scope, updated, desc, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( reservation.bNeedsNativeCreation && reservation.hBacking.m_nSlot != hFirst.m_nSlot );
			hSecond = reservation.hBacking;
			CHECK( registry.publishSharedDeviceBacking( hSecond, 501 ) == SIR_RESOURCE_RESULT_BUSY );
			CHECK( registry.publishSharedDeviceBacking( hSecond, 502 ) == SIR_RESOURCE_RESULT_SUCCESS );
			const sirSharedDeviceViewRequest_t request = { 90, 2, graph.getStats().nPublicationEpoch, 0, 64, 4, 3, 8, 1, 16, 2, 11 };
			CHECK( registry.acquireSharedDeviceView( scope, updated, request, &newer ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( older.nPhysicalBackingId != newer.nPhysicalBackingId );
			CHECK( older.nNativeResourceToken == 501 && newer.nNativeResourceToken == 502 );
		}
		SIRResourceDerivedStats stats = {};
		CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 256 && stats.nLiveSharedBorrows == 2 );
		CHECK( registry.retireRepresentation( hFirst, { 11, 4, 1, 2, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( older, { 11, 4, 3, 2, { 5, 7, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.updateProviderCompletion( { 11, 4, 1, 2, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( native.nReleases == 0 );
		CHECK( registry.updateProviderCompletion( { 11, 4, 2, 2, { 0, 7, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( native.nReleases == 1 && native.nLastToken == 501 );
		CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 128 && stats.nLiveSharedBorrows == 1 );
		CHECK( registry.retireRepresentation( hSecond, { 11, 4, 1, 2, { 7, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.releaseSharedDeviceView( newer, { 11, 4, 1, 2, { 7, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.updateProviderCompletion( { 11, 4, 1, 2, { 7, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( native.nReleases == 2 && native.nLastToken == 502 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView updated;
			CHECK( graph.resolve( scope, hNew, &updated ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceBackingDesc_t switched = desc;
			switched.nDeviceGeneration = 5;
			switched.nProviderId = 12;
			sirSharedDeviceBackingReservation_t reservation = {};
			CHECK( registry.reserveSharedDeviceBacking( scope, updated, switched, &reservation ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( reservation.bNeedsNativeCreation );
			CHECK( registry.publishSharedDeviceBacking( reservation.hBacking, 503 ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( registry.retireRepresentation( reservation.hBacking, { 12, 5, 1, 2, { 1, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( registry.updateProviderCompletion( { 12, 5, 1, 2, { 1, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( replacementDevice.nReleases == 1 && replacementDevice.nLastToken == 503 );
		}
		CHECK( pWriter->retire( hNew ) == SIR_RESOURCE_RESULT_SUCCESS );
		( void )pWriter->collectRetired( 8 );
	}
	CHECK( allocator.nLive == 0 );
}

int main( void )
{
	testSharedBackingLeases();
	testRevisionAndNativeToken();
	puts( "PASS: backing interning, cross-provider multi-lane completion, recook, cancellation and native token uniqueness" );
	return 0;
}
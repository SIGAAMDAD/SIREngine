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

#include <Streaming/SIRStreamingGRDGAdapter.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Platform/SIRPlatformFile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#include <malloc.h>
#endif

#define SIR_GRDG_TEST( condition )                                                                                                         \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "streaming GRDG test failed at %s:%d: %s\n", __FILE__, __LINE__, #condition );                                \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

typedef struct sirStreamingAdapterTestAllocator_s
{
	uint32_t nOutstanding;
} sirStreamingAdapterTestAllocator_t;

typedef struct sirStreamingAdapterTestProvider_s
{
	uint32_t nActivate;
	uint32_t nRetire;
} sirStreamingAdapterTestProvider_t;

static void *allocateGraphBytes( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nDomain;
	if ( !nBytes || nBytes > SIZE_MAX ) {
		return nullptr;
	}
	void *pResult = nullptr;
#if defined( _WIN32 )
	pResult = _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	const size_t nAlign = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	if ( posix_memalign( &pResult, nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
#endif
	if ( pResult ) {
		++( ( sirStreamingAdapterTestAllocator_t * )pContext )->nOutstanding;
	}
	return pResult;
}

static void freeGraphBytes( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
	--( ( sirStreamingAdapterTestAllocator_t * )pContext )->nOutstanding;
#if defined( _WIN32 )
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool beginActivate( void *pContext, sirStreamingTicket_t ticket, const sirStreamingGRDGResident_t *pResident ) noexcept
{
	( void )ticket;
	if ( !pResident || !pResident->hResource.m_nGeneration || !pResident->nRevision ) {
		return false;
	}
	++( ( sirStreamingAdapterTestProvider_t * )pContext )->nActivate;
	return true;
}

static bool beginRetire( void *pContext, sirStreamingTicket_t ticket, const sirStreamingGRDGResident_t *pResident ) noexcept
{
	( void )ticket;
	( void )pResident;
	++( ( sirStreamingAdapterTestProvider_t * )pContext )->nRetire;
	return true;
}

static sirStreamingGRDGBinding_t makeBinding( uint64_t nUnitId, uint64_t nAssetId, const char *pszPath ) noexcept
{
	sirStreamingGRDGBinding_t binding = {};
	binding.keyUnit.nContainerId = 91;
	binding.keyUnit.nUnitId = nUnitId;
	binding.keyUnit.nManifestGeneration = 7;
	binding.keyUnit.eKind = SIR_STREAMING_UNIT_WORLD_CELL;
	binding.pszFilePath = pszPath;
	binding.request.keyResource = { nAssetId, 31, 0 };
	binding.request.keyContent = { UINT64_C( 0xABCD ), UINT64_C( 0x1234 ), 4, 0 };
	binding.request.idChunk = { 0 };
	binding.request.eKind = SIR_RESOURCE_KIND_BLOB;
	binding.request.eMode = SIR_RESOURCE_FILE_READ_AT;
	binding.request.nSchemaVersion = 1;
	binding.request.nFormatId = 4;
	binding.request.nAlignment = 8;
	binding.request.nStagingDomain = 6;
	binding.request.nFileBytes = 128;
	return binding;
}

static sirStreamingDemand_t makeDemand( const sirStreamingGRDGBinding_t &binding, uint64_t nConsumerId ) noexcept
{
	sirStreamingDemand_t demand = {};
	demand.keyUnit = binding.keyUnit;
	demand.nConsumerId = nConsumerId;
	demand.nConsumerGeneration = 1;
	demand.nRequestEpoch = 1;
	demand.eTarget = SIR_STREAMING_TARGET_ACTIVE;
	return demand;
}

static SIRResourceGraphConfig makeGraphConfig( sirStreamingAdapterTestAllocator_t &allocator ) noexcept
{
	SIRResourceGraphConfig config = {};
	config.m_Allocator = { &allocator, allocateGraphBytes, freeGraphBytes };
	config.m_nMaxResources = 8;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 4 * 1024 * 1024;
	return config;
}

static void writeFixture( const char *pszPath ) noexcept
{
	uint8_t szBytes[128] = {};
	for ( uint32_t iByte = 0; iByte < 128; ++iByte ) {
		szBytes[iByte] = ( uint8_t )( iByte * 11 + 17 );
	}
	SIRPlatformFile file;
	SIR_GRDG_TEST( file.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	uint64_t nTransferred = 0;
	SIR_GRDG_TEST( file.writeAt( 0, szBytes, 128, &nTransferred ) == SIR_FILE_OK && nTransferred == 128 );
	SIR_GRDG_TEST( file.close() == SIR_FILE_OK );
}

static void testPhysicalSharing( const char *pszPath ) noexcept
{
	sirStreamingAdapterTestAllocator_t allocator = {};
	SIRResourceGraph graph( makeGraphConfig( allocator ) );
	SIR_GRDG_TEST( graph.isReady() );
	SIRMemoryManager memory;
	SIR_GRDG_TEST( memory.init( { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 } ) );
	SIRTaskScheduler scheduler;
	SIR_GRDG_TEST( scheduler.init( &memory, { 16, 0, 32 } ) );

	alignas( 64 ) uint8_t szCoordinator[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	alignas( 64 ) uint8_t szGRDG[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIR_GRDG_TEST( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) == SIR_STREAMING_OK );
	const sirStreamingGRDGBinding_t binding = makeBinding( 200, 900, pszPath );
	sirStreamingAdapterTestProvider_t provider = {};
	SIRStreamingGRDGAdapter adapter;
	sirStreamingGRDGConfig_t config = {};
	config.pGraph = &graph;
	config.pScheduler = &scheduler;
	config.pCoordinator = &coordinator;
	config.pBindings = &binding;
	config.nBindingCount = 1;
	config.pStorage = szGRDG;
	config.nStorageBytes = sizeof( szGRDG );
	config.nCapacity = 2;
	config.nProviderId = 88;
	config.callbacks = { &provider, beginActivate, beginRetire };
	SIR_GRDG_TEST( adapter.requiredBytes( 2 ) <= sizeof( szGRDG ) );
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_OK );
	SIRStreamingProviderBridge bridge;
	SIR_GRDG_TEST( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), adapter.getBridgeCallbacks() ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.bindBridge( &bridge ) == SIR_STREAMING_OK );

	sirStreamingDemand_t demand = makeDemand( binding, 1 );
	sirStreamingTicket_t first = {};
	sirStreamingTicket_t second = {};
	SIR_GRDG_TEST( coordinator.submit( demand, &first ) == SIR_STREAMING_OK );
	demand.nConsumerId = 2;
	SIR_GRDG_TEST( coordinator.submit( demand, &second ) == SIR_STREAMING_OK );
	sirStreamingBridgeStats_t bridgeStats = {};
	sirStreamingGRDGStats_t grdgStats = {};
	SIR_GRDG_TEST( bridge.pump( 2, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nPrepareDispatched == 2 );
	SIR_GRDG_TEST( adapter.pump( 2, &grdgStats ) == SIR_STREAMING_OK && grdgStats.nStarted == 1 );
	SIR_GRDG_TEST( scheduler.pumpCPU( 8 ) == 1 );
	SIR_GRDG_TEST( adapter.pump( 2, &grdgStats ) == SIR_STREAMING_OK && grdgStats.nPrepared == 2 );
	sirStreamingGRDGResident_t firstResource = {};
	sirStreamingGRDGResident_t secondResource = {};
	SIR_GRDG_TEST( adapter.queryResource( first, &firstResource ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.queryResource( second, &secondResource ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( firstResource.hResource.m_nSlot == secondResource.hResource.m_nSlot &&
	               firstResource.hResource.m_nGeneration == secondResource.hResource.m_nGeneration &&
	               firstResource.nRevision == secondResource.nRevision );
	SIR_GRDG_TEST( graph.getStats().m_nUniqueChunks == 1 && graph.getStats().m_nPayloadBytes == 128 );

	SIR_GRDG_TEST( bridge.pump( 2, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nActivationDispatched == 2 );
	SIR_GRDG_TEST( provider.nActivate == 2 );
	SIR_GRDG_TEST( adapter.acknowledgeActivated( first, true ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.acknowledgeActivated( second, true ) == SIR_STREAMING_OK );

	// A same-handle revision replacement invalidates both provider pins without
	// releasing the previous immutable record before fence acknowledgement.
	const SIRResourcePublication replacement = { binding.request.keyResource, SIR_RESOURCE_KIND_BLOB, 1, 4, nullptr, 0, nullptr, 0 };
	SIRResourceHandle hReplacement = {};
	SIR_GRDG_TEST( graph.getWriter()->publish( replacement, &hReplacement ) == SIR_RESOURCE_RESULT_SUCCESS );
	SIR_GRDG_TEST( adapter.pump( 2, &grdgStats ) == SIR_STREAMING_OK );
	sirStreamingSnapshot_t firstSnapshot = {};
	sirStreamingSnapshot_t secondSnapshot = {};
	SIR_GRDG_TEST( coordinator.query( first, &firstSnapshot ) == SIR_STREAMING_OK && firstSnapshot.bCancellationRequested );
	SIR_GRDG_TEST( coordinator.query( second, &secondSnapshot ) == SIR_STREAMING_OK && secondSnapshot.bCancellationRequested );
	SIR_GRDG_TEST( bridge.pump( 2, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nRetirementDispatched == 2 );
	SIR_GRDG_TEST( provider.nRetire == 2 );
	SIR_GRDG_TEST( adapter.acknowledgeRetired( first ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.acknowledgeRetired( second ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( coordinator.getStats().nOccupied == 0 );
	SIR_GRDG_TEST( adapter.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( bridge.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( coordinator.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( graph.getWriter()->retire( hReplacement ) == SIR_RESOURCE_RESULT_SUCCESS );
	graph.getWriter()->collectRetired( 32 );
	SIR_GRDG_TEST( scheduler.shutdown() );
	memory.shutdown();
	// All GRDG metadata allocations are released when graph goes out of scope.
}

static void testCancellationAndMissingFile( const char *pszPath ) noexcept
{
	sirStreamingAdapterTestAllocator_t allocator = {};
	SIRResourceGraph graph( makeGraphConfig( allocator ) );
	SIRMemoryManager memory;
	SIR_GRDG_TEST( memory.init( { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 } ) );
	SIRTaskScheduler scheduler;
	SIR_GRDG_TEST( scheduler.init( &memory, { 16, 0, 32 } ) );

	alignas( 64 ) uint8_t szCoordinator[4096] = {};
	alignas( 64 ) uint8_t szBridge[4096] = {};
	alignas( 64 ) uint8_t szGRDG[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIR_GRDG_TEST( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 1 ) == SIR_STREAMING_OK );
	sirStreamingGRDGBinding_t binding = makeBinding( 300, 901, pszPath );
	sirStreamingAdapterTestProvider_t provider = {};
	SIRStreamingGRDGAdapter adapter;
	sirStreamingGRDGConfig_t config = {};
	config.pGraph = &graph;
	config.pScheduler = &scheduler;
	config.pCoordinator = &coordinator;
	config.pBindings = &binding;
	config.nBindingCount = 1;
	config.pStorage = szGRDG;
	config.nStorageBytes = sizeof( szGRDG );
	config.nCapacity = 1;
	config.nProviderId = 88;
	config.callbacks = { &provider, beginActivate, beginRetire };
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_OK );
	SIRStreamingProviderBridge bridge;
	SIR_GRDG_TEST( bridge.initialize( &coordinator, szBridge, sizeof( szBridge ), adapter.getBridgeCallbacks() ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.bindBridge( &bridge ) == SIR_STREAMING_OK );

	sirStreamingTicket_t ticket = {};
	const sirStreamingDemand_t demand = makeDemand( binding, 10 );
	SIR_GRDG_TEST( coordinator.submit( demand, &ticket ) == SIR_STREAMING_OK );
	sirStreamingBridgeStats_t bridgeStats = {};
	sirStreamingGRDGStats_t grdgStats = {};
	SIR_GRDG_TEST( bridge.pump( 1, &bridgeStats ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.pump( 1, &grdgStats ) == SIR_STREAMING_OK && grdgStats.nStarted == 1 );
	SIR_GRDG_TEST( coordinator.cancel( ticket ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( bridge.pump( 1, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nCancellationDispatched == 1 );
	SIR_GRDG_TEST( scheduler.pumpCPU( 8 ) == 0 );
	SIR_GRDG_TEST( adapter.pump( 1, &grdgStats ) == SIR_STREAMING_OK && grdgStats.nFailed == 1 );
	SIR_GRDG_TEST( bridge.pump( 1, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nRetirementDispatched == 1 );
	SIR_GRDG_TEST( adapter.acknowledgeRetired( ticket ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( coordinator.getStats().nOccupied == 0 );

	binding = makeBinding( 300, 902, "sir_streaming_grdg_missing_fixture.bin" );
	SIR_GRDG_TEST( coordinator.submit( demand, &ticket ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( bridge.pump( 1, &bridgeStats ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.pump( 1, &grdgStats ) == SIR_STREAMING_OK && grdgStats.nFailed == 1 );
	SIR_GRDG_TEST( bridge.pump( 1, &bridgeStats ) == SIR_STREAMING_OK && bridgeStats.nRetirementDispatched == 1 );
	SIR_GRDG_TEST( adapter.acknowledgeRetired( ticket ) == SIR_STREAMING_OK );
	SIR_GRDG_TEST( adapter.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( bridge.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( coordinator.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( scheduler.shutdown() );
	memory.shutdown();
}

static void testDirectoryValidation( const char *pszPath ) noexcept
{
	sirStreamingAdapterTestAllocator_t allocator = {};
	SIRResourceGraph graph( makeGraphConfig( allocator ) );
	SIRMemoryManager memory;
	SIR_GRDG_TEST( memory.init( { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 } ) );
	SIRTaskScheduler scheduler;
	SIR_GRDG_TEST( scheduler.init( &memory, { 8, 0, 16 } ) );

	alignas( 64 ) uint8_t szCoordinator[4096] = {};
	alignas( 64 ) uint8_t szGRDG[8192] = {};
	SIRStreamingCoordinator coordinator;
	SIR_GRDG_TEST( coordinator.initialize( szCoordinator, sizeof( szCoordinator ), 2 ) == SIR_STREAMING_OK );
	sirStreamingGRDGBinding_t bindings[2] = { makeBinding( 2, 1, pszPath ), makeBinding( 1, 2, pszPath ) };
	sirStreamingAdapterTestProvider_t provider = {};
	SIRStreamingGRDGAdapter adapter;
	sirStreamingGRDGConfig_t config = {};
	config.pGraph = &graph;
	config.pScheduler = &scheduler;
	config.pCoordinator = &coordinator;
	config.pBindings = bindings;
	config.nBindingCount = 2;
	config.pStorage = szGRDG;
	config.nStorageBytes = sizeof( szGRDG );
	config.nCapacity = 2;
	config.nProviderId = 7;
	config.callbacks = { &provider, beginActivate, beginRetire };

	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	config.pStorage = bindings;
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	config.pStorage = szGRDG;
	bindings[1].keyUnit = bindings[0].keyUnit;
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	bindings[0] = makeBinding( 1, 1, pszPath );
	bindings[1] = makeBinding( 2, 2, pszPath );
	config.nProviderId = 0;
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	config.nProviderId = 7;
	bindings[1].request.nAlignment = 3;
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	bindings[1].request.nAlignment = 8;
	bindings[1].request.nFileBytes = 0;
	SIR_GRDG_TEST( adapter.initialize( config ) == SIR_STREAMING_INVALID_ARGUMENT );
	SIR_GRDG_TEST( coordinator.shutdown() == SIR_STREAMING_OK );
	SIR_GRDG_TEST( scheduler.shutdown() );
	memory.shutdown();
}

int main( void )
{
	const char *pszPath = "sir_streaming_grdg_fixture.bin";
	SIRPlatformFile::remove( pszPath );
	writeFixture( pszPath );
	testPhysicalSharing( pszPath );
	testCancellationAndMissingFile( pszPath );
	testDirectoryValidation( pszPath );
	SIR_GRDG_TEST( SIRPlatformFile::remove( pszPath ) == SIR_FILE_OK );
	printf( "PASS: canonical GRDG file loads, shared pins, activation and cancellation\n" );
	return 0;
}

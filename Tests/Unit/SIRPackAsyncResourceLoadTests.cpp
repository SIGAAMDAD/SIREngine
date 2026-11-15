/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/IO/SIRAsyncIOService.h>
#include <Prediction/Budget/SIRAdaptiveWorkBudgeter.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Resources/SIRPackAsyncResourceLoad.h>
#include <Resources/SIRPackCodec.h>
#include <Resources/SIRPackSourceMount.h>
#include <Streaming/SIRStreamingPackDispatcher.h>
#include <Tools/SIRPackCooker/SIRPackCooker.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#include <malloc.h>
#endif

#define REQUIRE( x )                                                                                                                       \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "assertion at %d: %s\n", __LINE__, #x );                                                                      \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static void *sirPackAllocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nDomain;
	if ( nBytes > SIZE_MAX ) {
		return nullptr;
	}
#if defined( _WIN32 )
	return _aligned_malloc( ( size_t )nBytes, nAlignment );
#else
	void *pMemory = nullptr;
	const size_t nAligned = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	return posix_memalign( &pMemory, nAligned, ( size_t )nBytes ) == 0 ? pMemory : nullptr;
#endif
}

static void sirPackFree( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )pContext;
	( void )nBytes;
	( void )nAlignment;
	( void )nDomain;
#if defined( _WIN32 )
	_aligned_free( pData );
#else
	free( pData );
#endif
}

static bool sirPackWriteFile( const char *szPath, const uint8_t *pData, uint64_t nBytes ) noexcept
{
	SIRPlatformFile file;
	if ( file.open( szPath, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_TRUNCATE ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t nWritten = 0;
	const sirFileResult_t eResult = file.write( pData, nBytes, &nWritten );
	file.close();
	return eResult == SIR_FILE_OK && nWritten == nBytes;
}

static bool sirPackPump( SIRAsyncIOService& io, SIRPackAsyncResourceLoad& loader, SIRResourceHandle *pHandle ) noexcept
{
	for ( uint32_t iSpin = 0; iSpin < 3000000; ++iSpin ) {
		sirIOCompletion_t completion = {};
		if ( io.poll( &completion, 1 ) != 1 ) {
			continue;
		}
		if ( !loader.ownsCompletion( completion ) ) {
			return false;
		}
		const sirResourceResult_t eComplete = loader.complete( completion, pHandle );
		if ( eComplete == SIR_RESOURCE_RESULT_SUCCESS ) {
			return true;
		}
		if ( eComplete != SIR_RESOURCE_RESULT_BUSY ) {
			fprintf( stderr, "pack loader failed: %u\n", ( unsigned )eComplete );
			return false;
		}
	}
	return false;
}

static bool sirPackBudgetReserve( void *pContext, const sirStreamingPackBudgetDemand_t& demand, uint64_t *pReservation ) noexcept
{
	SIRAdaptiveWorkBudgeter *pBudget = ( SIRAdaptiveWorkBudgeter * )pContext;
	sirWorkBudgetRequest_t request = {};
	request.key = demand.key;
	request.nRevision = demand.nRevision == 0 ? 1 : demand.nRevision;
	request.eClass = demand.bMandatory ? SIR_WORK_BUDGET_MANDATORY : SIR_WORK_BUDGET_OPTIONAL;
	request.cost.nIOBytes = 10;
	request.cost.nRamBytes = 20;
	sirWorkBudgetHandle_t hHandle = {};
	if ( pBudget->admit( request, &hHandle ) != SIR_WORK_BUDGET_OK ) {
		return false;
	}
	*pReservation = ( ( uint64_t )hHandle.nGeneration << 32 ) | hHandle.nSlot;
	return true;
}

static void sirPackBudgetRelease( void *pContext, uint64_t nReservation ) noexcept
{
	SIRAdaptiveWorkBudgeter *pBudget = ( SIRAdaptiveWorkBudgeter * )pContext;
	const sirWorkBudgetHandle_t hHandle = { ( uint32_t )nReservation, ( uint32_t )( nReservation >> 32 ) };
	pBudget->release( hHandle );
}

static bool testCompressedPackPublication( void ) noexcept
{
	static constexpr uint8_t s_szFirst[] = { 'h', 'e', 'l', 'l', 'o' };
	static constexpr uint8_t s_szSecond[] = { 'w', 'o', 'r', 'l', 'd' };
	uint8_t szEncoded[128] = {};
	uint64_t nEncoded = 0;
	sirPackCodec_t eCodec = SIR_PACK_CODEC_LZ4;
	if ( sirPackEncodeOffline( eCodec, s_szSecond, sizeof( s_szSecond ), szEncoded, sizeof( szEncoded ), &nEncoded ) != SIR_PACK_OK ) {
		eCodec = SIR_PACK_CODEC_NONE;
		REQUIRE( sirPackEncodeOffline( eCodec, s_szSecond, sizeof( s_szSecond ), szEncoded, sizeof( szEncoded ), &nEncoded ) ==
		         SIR_PACK_OK );
	}

	const SIRResourceKey key = { 999, 1, 0 };
	sirPackCookResource_t resource = { key, 7, 1, 0, 2, 0, 0, 1, SIR_RESOURCE_KIND_BLOB };
	sirPackCookChunk_t chunks[2] = { { 0, 0, s_szFirst, sizeof( s_szFirst ), 16 }, { 1, 0, s_szSecond, sizeof( s_szSecond ), 16 } };
	if ( eCodec != SIR_PACK_CODEC_NONE ) {
		chunks[1].eCodec = eCodec;
		chunks[1].nCodecVersion = 1;
		chunks[1].pEncodedBytes = szEncoded;
		chunks[1].nEncodedBytes = nEncoded;
	}
	uint8_t szShard[512] = {};
	uint8_t szCatalog[512] = {};
	uint8_t szCatalogScratch[256] = {};
	sirPackCookShard_t shard = { szShard, sizeof( szShard ) };
	sirPackCookDesc_t desc = { 55, 9, &resource, 1, chunks, 2, nullptr, 0, &shard, 1, szCatalogScratch, sizeof( szCatalogScratch ) };
	REQUIRE( SIRPackCooker::build( desc, szCatalog, sizeof( szCatalog ) ) == SIR_PACK_OK );

	SIRPackCatalog catalog;
	REQUIRE( catalog.open( szCatalog, SIRPackCooker::requiredCatalogBytes( desc ), szCatalogScratch, sizeof( szCatalogScratch ) ) ==
	         SIR_PACK_OK );
	const uint64_t nShardBytes = sirPackRead64( szShard + 32 );
	REQUIRE( catalog.validateShard( 0, szShard, nShardBytes ) == SIR_PACK_OK );
	const char *szPath = "sir-pack-async-fixture.sirp";
	REQUIRE( sirPackWriteFile( szPath, szShard, nShardBytes ) );
	uint8_t szBadShard[512] = {};
	memcpy( szBadShard, szShard, ( size_t )nShardBytes );
	sirPackWrite64( szBadShard + 24, 99 );
	REQUIRE( sirPackWriteFile( "sir-pack-async-stale.sirp", szBadShard, nShardBytes ) );

	SIRAsyncIOService io;
	const uint64_t nIOStorage = SIRAsyncIOService::requiredBytes( 4, 2, 2 );
	void *pIOStorage = malloc( ( size_t )nIOStorage );
	REQUIRE( pIOStorage != nullptr && io.initialize( pIOStorage, nIOStorage, 4, 2, 2 ) );
	sirIOSourceHandle_t hShard = {};
	const char *szShardPaths[] = { szPath };
	SIRPackSourceMount mount;
	const char *szBadPaths[] = { "sir-pack-async-stale.sirp" };
	REQUIRE( mount.mount( io, catalog, szBadPaths, &hShard, 1 ) == SIR_PACK_STALE_SOURCE );
	REQUIRE( !mount.isMounted() );
	REQUIRE( mount.mount( io, catalog, szShardPaths, &hShard, 1 ) == SIR_PACK_OK );

	SIRResourceGraphConfig config = {};
	config.m_Allocator = { nullptr, sirPackAllocate, sirPackFree };
	config.m_nMetadataDomain = 1;
	config.m_nMaxResources = 16;
	config.m_nMaxStaging = 4;
	config.m_nMaxProviderPins = 4;
	config.m_nMaxCommittedBytes = 4 * 1024 * 1024;
	{
		SIRResourceGraph graph( config );
		REQUIRE( graph.isReady() );
		SIRPackAsyncResourceLoad loader;
		SIRResourceChunkPublication publications[2] = {};
		SIRResourceDependency dependencies[1] = {};
		uint8_t szIngress[256] = {};
		const uint64_t nScratch = sirPackDecodeScratchBytes( eCodec );
		void *pScratch = nScratch > 0 ? malloc( ( size_t )nScratch ) : nullptr;
		REQUIRE( nScratch == 0 || pScratch != nullptr );
		const sirPackAsyncWorkspace_t workspace = { publications, 2, dependencies, 1, szIngress, sizeof( szIngress ), pScratch, nScratch };
		REQUIRE( loader.begin( graph, io, catalog, &hShard, 1, key, 0, workspace ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( loader.retainSubscriber( key, 0 ) );
		REQUIRE( loader.withdrawSubscriber() == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceHandle hResource = {};
		REQUIRE( sirPackPump( io, loader, &hResource ) );
		REQUIRE( !loader.isActive() );
		REQUIRE( graph.getStats().m_nUniqueChunks == 2 );
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView view;
			REQUIRE( graph.resolve( scope, hResource, &view ) == SIR_RESOURCE_RESULT_SUCCESS );
			SIRResourceConstBytes first = {};
			SIRResourceConstBytes second = {};
			REQUIRE( view.getChunk( { 0 }, &first ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( view.getChunk( { 1 }, &second ) == SIR_RESOURCE_RESULT_SUCCESS );
			REQUIRE( first.m_nSize == sizeof( s_szFirst ) && second.m_nSize == sizeof( s_szSecond ) );
			REQUIRE( memcmp( first.m_pData, s_szFirst, first.m_nSize ) == 0 );
			REQUIRE( memcmp( second.m_pData, s_szSecond, second.m_nSize ) == 0 );
		}
		// Same digest/revision reuses the canonical physical backing.
		REQUIRE( loader.begin( graph, io, catalog, &hShard, 1, key, 1, workspace ) == SIR_RESOURCE_RESULT_SUCCESS );
		REQUIRE( sirPackPump( io, loader, &hResource ) );
		REQUIRE( graph.getStats().m_nUniqueChunks == 2 );

		// Two different streaming consumers now request one canonical resource.
		// The dispatcher issues one physical read and keeps it alive when one
		// consumer withdraws while the other still needs its payload.
		alignas( 64 ) uint8_t szStreamingStorage[1024] = {};
		SIRStreamingCoordinator coordinator;
		REQUIRE( coordinator.initialize( szStreamingStorage, sizeof( szStreamingStorage ), 4 ) == SIR_STREAMING_OK );
		sirStreamingPackLoadState_t loadState[1];
		sirStreamingPackTicketState_t ticketState[4] = {};
		const sirStreamingUnitKey_t unit = { 55, 91, 9, 0, SIR_STREAMING_UNIT_RESOURCE_BUNDLE, 0 };
		const sirStreamingPackBinding_t binding = { unit, key, 2 };
		alignas( 64 ) uint8_t szBudgetStorage[4096] = {};
		SIRAdaptiveWorkBudgeter budget;
		sirWorkBudgetConfig_t budgetConfig = {};
		budgetConfig.nCapacity = 4;
		budgetConfig.limits.nIOBytes = 10;
		budgetConfig.limits.nRamBytes = 20;
		REQUIRE( budget.initialize( szBudgetStorage, sizeof( szBudgetStorage ), budgetConfig ) == SIR_WORK_BUDGET_OK );
		sirStreamingPackConfig_t dispatchConfig = { &coordinator, &graph,      &io, &catalog, &hShard, &binding, &workspace,
		                                            loadState,    ticketState, 1,   1,        1,       4 };
		dispatchConfig.pBudgetContext = &budget;
		dispatchConfig.pfnReserveBudget = sirPackBudgetReserve;
		dispatchConfig.pfnReleaseBudget = sirPackBudgetRelease;
		SIRStreamingPackDispatcher dispatcher;
		REQUIRE( dispatcher.initialize( dispatchConfig ) == SIR_STREAMING_OK );
		sirStreamingDemand_t demand = {};
		demand.keyUnit = unit;
		demand.nConsumerId = 100;
		demand.nConsumerGeneration = 1;
		demand.nRequestEpoch = 1;
		demand.nPriority = 220;
		demand.eTarget = SIR_STREAMING_TARGET_PREPARED;
		sirStreamingTicket_t ticketA = {};
		sirStreamingTicket_t ticketB = {};
		REQUIRE( coordinator.submit( demand, &ticketA ) == SIR_STREAMING_OK );
		demand.nConsumerId = 200;
		demand.nPriority = 200;
		REQUIRE( coordinator.submit( demand, &ticketB ) == SIR_STREAMING_OK );
		sirStreamingPackStats_t dispatchStats = {};
		const sirWorkBudgetRequest_t competing = { { 400, 2, 0 }, 1, 0, 0, SIR_WORK_BUDGET_MANDATORY, { 0, 0, 10, 0, 20, 0, 0, 0 } };
		sirWorkBudgetHandle_t hCompeting = {};
		REQUIRE( budget.admit( competing, &hCompeting ) == SIR_WORK_BUDGET_OK );
		REQUIRE( dispatcher.pump( 2, &dispatchStats ) == SIR_STREAMING_OK );
		REQUIRE( dispatchStats.nDeferred == 1 && dispatcher.getActiveLoadCount() == 0 );
		REQUIRE( budget.release( hCompeting ) == SIR_WORK_BUDGET_OK );
		REQUIRE( dispatcher.pump( 2, &dispatchStats ) == SIR_STREAMING_OK );
		REQUIRE( dispatchStats.nStarted == 1 && dispatchStats.nShared == 1 );
		REQUIRE( budget.getUsed().nIOBytes == 10 && budget.getUsed().nRamBytes == 20 );
		REQUIRE( dispatcher.getActiveLoadCount() == 1 );
		REQUIRE( coordinator.cancel( ticketA ) == SIR_STREAMING_OK );
		REQUIRE( dispatcher.pump( 0, &dispatchStats ) == SIR_STREAMING_OK );
		REQUIRE( dispatchStats.nCancelled == 1 );
		bool bPublished = false;
		for ( uint32_t iSpin = 0; iSpin < 3000000 && !bPublished; ++iSpin ) {
			sirIOCompletion_t completion = {};
			if ( io.poll( &completion, 1 ) == 0 ) {
				continue;
			}
			REQUIRE( dispatcher.ownsCompletion( completion ) );
			REQUIRE( dispatcher.complete( completion, &hResource ) == SIR_STREAMING_OK );
			bPublished = dispatcher.getActiveLoadCount() == 0;
		}
		REQUIRE( bPublished );
		REQUIRE( budget.getUsed().nIOBytes == 0 && budget.getUsed().nRamBytes == 0 );
		sirStreamingSnapshot_t snapshot = {};
		REQUIRE( coordinator.query( ticketA, &snapshot ) == SIR_STREAMING_STALE_TICKET );
		REQUIRE( coordinator.query( ticketB, &snapshot ) == SIR_STREAMING_OK );
		REQUIRE( snapshot.eStage == SIR_STREAMING_STAGE_PREPARED );
		REQUIRE( graph.getStats().m_nUniqueChunks == 2 );
		REQUIRE( dispatcher.retirePrepared( ticketB ) == SIR_STREAMING_OK );
		// Already-published revision uses the same immutable source without
		// scheduling an OS read, decoding scratch or a second staging allocation.
		demand.nConsumerId = 300;
		sirStreamingTicket_t ticketC = {};
		REQUIRE( coordinator.submit( demand, &ticketC ) == SIR_STREAMING_OK );
		REQUIRE( dispatcher.pump( 1, &dispatchStats ) == SIR_STREAMING_OK );
		REQUIRE( dispatchStats.nCacheHits == 1 && dispatchStats.nStarted == 0 );
		REQUIRE( dispatcher.getActiveLoadCount() == 0 );
		REQUIRE( coordinator.query( ticketC, &snapshot ) == SIR_STREAMING_OK );
		REQUIRE( snapshot.eStage == SIR_STREAMING_STAGE_PREPARED );
		REQUIRE( dispatcher.retirePrepared( ticketC ) == SIR_STREAMING_OK );
		REQUIRE( dispatcher.shutdown() == SIR_STREAMING_OK );
		REQUIRE( budget.shutdown() == SIR_WORK_BUDGET_OK );
		REQUIRE( coordinator.shutdown() == SIR_STREAMING_OK );

		free( pScratch );
	}
	REQUIRE( mount.unmount() == SIR_PACK_OK );
	io.shutdown();
	free( pIOStorage );
	SIRPlatformFile::remove( szPath );
	SIRPlatformFile::remove( "sir-pack-async-stale.sirp" );
	return true;
}

int main( void )
{
	return testCompressedPackPublication() ? 0 : 1;
}

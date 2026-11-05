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

#include <Scripting/Core/SIRScriptOptimizationCoordinator.h>
#include <Scripting/Core/SIRScriptBindingRegistry.h>

#include <chrono>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#if defined( _WIN32 )
#include <malloc.h>
#endif

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression );                                                             \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirOptimizerTestMemory_s
{
	uint32_t nLive;
	uint32_t nAcquired;
	uint32_t nReleased;
	SIRScriptOptimizationCoordinator *pCoordinator;
	sirScriptResult_t eReentrant;
} sirOptimizerTestMemory_t;

static sirScriptResult_t optimizerTestAllocate( void *pUser, uint32_t nBytes, uint32_t nAlignment,
                                                sirScriptPayloadAllocation_t *pAllocation ) noexcept
{
	sirOptimizerTestMemory_t *pMemory = ( sirOptimizerTestMemory_t * )pUser;
	if ( pMemory->pCoordinator != nullptr ) {
		sirScriptOptimizationJobInfo_t info = {};
		pMemory->eReentrant = pMemory->pCoordinator->query( { 1, 1 }, &info );
	}
	const size_t nAligned = nAlignment < sizeof( void * ) ? sizeof( void * ) : nAlignment;
	const size_t nSize = ( ( nBytes + nAligned - 1 ) / nAligned ) * nAligned;
#if defined( _WIN32 )
	void *pData = _aligned_malloc( nSize, nAligned );
#else
	void *pData = aligned_alloc( nAligned, nSize );
#endif
	if ( pData == nullptr ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	++pMemory->nLive;
	++pMemory->nAcquired;
	*pAllocation = { pData, pMemory->nAcquired };
	return SIR_SCRIPT_OK;
}

static void optimizerTestFree( void *pUser, sirScriptPayloadAllocation_t allocation ) noexcept
{
	sirOptimizerTestMemory_t *pMemory = ( sirOptimizerTestMemory_t * )pUser;
#if defined( _WIN32 )
	_aligned_free( allocation.pData );
#else
	free( allocation.pData );
#endif
	++pMemory->nReleased;
	--pMemory->nLive;
}

typedef struct sirOptimizerTestTransport_s
{
	SIRScriptArtifactCache *pCache;
	SIRScriptOptimizationCoordinator *pCoordinator;
	sirScriptResult_t eReentrant;
	uint32_t nStarts;
	uint32_t nStops;
	uint32_t nSent;
	uint32_t nDelivered;
	bool bRunning;
	bool bDeliver;
	bool bCorrupt;
	bool bSendFail;
	bool bReceiveFail;
	bool bReturnBackendFailure;
	bool bReenter;
	uint8_t szPending[80][SIRScriptOptimizationProtocol::REPLY_BYTES];
	uint32_t nFront;
	uint32_t nBack;
} sirOptimizerTestTransport_t;

static sirScriptResult_t optimizerTestStart( void *pUser ) noexcept
{
	sirOptimizerTestTransport_t *p = ( sirOptimizerTestTransport_t * )pUser;
	p->bRunning = true;
	++p->nStarts;
	return SIR_SCRIPT_OK;
}

static void optimizerTestStop( void *pUser ) noexcept
{
	sirOptimizerTestTransport_t *p = ( sirOptimizerTestTransport_t * )pUser;
	p->bRunning = false;
	p->nFront = p->nBack = 0;
	++p->nStops;
}

static sirScriptResult_t optimizerTestSend( void *pUser, const uint8_t *pBytes, uint32_t nBytes ) noexcept
{
	sirOptimizerTestTransport_t *p = ( sirOptimizerTestTransport_t * )pUser;
	if ( !p->bRunning || p->bSendFail ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( p->nBack >= 80 ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( p->bReenter && p->pCoordinator != nullptr ) {
		sirScriptOptimizationJobInfo_t info = {};
		p->eReentrant = p->pCoordinator->query( { 1, 1 }, &info );
	}
	sirScriptOptimizationRequest_t request = {};
	if ( SIRScriptOptimizationProtocol::decodeRequest( pBytes, nBytes, &request ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	sirScriptOptimizationReply_t reply = {};
	reply.nJobId = request.nJobId;
	reply.eResult = p->bReturnBackendFailure ? SIR_SCRIPT_BACKEND_FAILURE : SIR_SCRIPT_OK;
	if ( reply.eResult == SIR_SCRIPT_OK ) {
		alignas( 64 ) uint8_t szOutput[256] = {};
		for ( uint32_t i = 0; i < 256; ++i ) {
			szOutput[i] = ( uint8_t )( request.artifact.nRevision + i );
		}
		if ( p->pCache->store( request.artifact, request.key, szOutput, sizeof( szOutput ), 64, &reply.cached ) != SIR_SCRIPT_OK ) {
			return SIR_SCRIPT_BACKEND_FAILURE;
		}
	}
	if ( SIRScriptOptimizationProtocol::encodeReply( reply, p->szPending[p->nBack] ) != SIR_SCRIPT_OK ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( p->bCorrupt ) {
		p->szPending[p->nBack][25] ^= 0x80;
	}
	++p->nBack;
	++p->nSent;
	return SIR_SCRIPT_OK;
}

static sirScriptResult_t optimizerTestReceive( void *pUser, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pReceived ) noexcept
{
	sirOptimizerTestTransport_t *p = ( sirOptimizerTestTransport_t * )pUser;
	if ( p->bReceiveFail ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	if ( !p->bRunning || !p->bDeliver || p->nFront == p->nBack ) {
		return SIR_SCRIPT_BUSY;
	}
	if ( nCapacity != SIRScriptOptimizationProtocol::REPLY_BYTES ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	memcpy( pBytes, p->szPending[p->nFront++], nCapacity );
	*pReceived = nCapacity;
	++p->nDelivered;
	return SIR_SCRIPT_OK;
}

static sirScriptOptimizationTransportAPI_t optimizerTestAPI( sirOptimizerTestTransport_t *pFixture ) noexcept
{
	return {
	    SIR_SCRIPT_OPT_TRANSPORT_ABI_REVISION, pFixture, optimizerTestStart, optimizerTestStop, optimizerTestSend, optimizerTestReceive };
}

static sirScriptOptimizationRequest_t optimizerTestRequest( uint64_t nRevision, uint64_t nSchema ) noexcept
{
	sirScriptOptimizationRequest_t request = {};
	request.artifact = { 1, 22, nRevision, 77, nSchema, 88, 100 + nRevision };
	char szRecipe[128] = {};
	const int nRecipe = snprintf( szRecipe, sizeof( szRecipe ), "v1/frontend=any/revision=%llu/schema=%llu",
	                              ( unsigned long long )nRevision, ( unsigned long long )nSchema );
	if ( nRecipe > 0 && ( size_t )nRecipe < sizeof( szRecipe ) ) {
		( void )SIRScriptArtifactCache::makeKey( szRecipe, ( uint32_t )nRecipe, &request.key );
	}
	request.nInputBytes = 128;
	request.eTier = SIR_SCRIPT_OPT_TARGET_CPU;
	return request;
}

static bool testLifecycleAndRollback( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-optimizer-jobs-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( root.string().c_str(), 1024 ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry registry;
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	sirOptimizerTestMemory_t memory = {};
	sirScriptPayloadAllocatorAPI_t allocator = {
	    SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, &memory, 1024, 64, optimizerTestAllocate, optimizerTestFree };
	SIRScriptCachedProgramSource source;
	SIR_TEST_CHECK( source.configure( &cache, allocator, 1024 ) == SIR_SCRIPT_OK );
	SIRScriptCodeManager manager( &registry );
	SIR_TEST_CHECK( manager.configure( 88, source.makeProgramSourceAPI() ) == SIR_SCRIPT_OK );
	sirOptimizerTestTransport_t transport = {};
	transport.pCache = &cache;
	transport.bDeliver = true;
	transport.bReenter = true;
	SIRScriptOptimizationCoordinator coordinator;
	transport.pCoordinator = &coordinator;
	memory.pCoordinator = &coordinator;
	sirScriptOptimizationTransportAPI_t api = optimizerTestAPI( &transport );
	SIR_TEST_CHECK( coordinator.configure( api, nullptr, &manager ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( coordinator.configure( api, &source, &manager ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.configure( api, &source, &manager ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( transport.nStarts == 1 );
	const sirScriptOptimizationRequest_t first = optimizerTestRequest( 1, registry.schemaFingerprint() );
	sirScriptOptimizationJobHandle_t hBad = {};
	sirScriptOptimizationRequest_t invalid = first;
	invalid.nJobId = 100;
	SIR_TEST_CHECK( coordinator.submit( invalid, true, &hBad ) == SIR_SCRIPT_INVALID_ARGUMENT );
	invalid = first;
	invalid.nInputBytes = 0;
	SIR_TEST_CHECK( coordinator.submit( invalid, true, &hBad ) == SIR_SCRIPT_INVALID_ARGUMENT );

	// Queue cancellation never dispatches to the transport.
	sirScriptOptimizationJobHandle_t hQueued = {};
	SIR_TEST_CHECK( coordinator.submit( first, true, &hQueued ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.cancel( hQueued ) == SIR_SCRIPT_OK );
	sirScriptOptimizationJobInfo_t info = {};
	SIR_TEST_CHECK( coordinator.query( hQueued, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_CANCELED );
	SIR_TEST_CHECK( coordinator.release( hQueued ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hQueued, &info ) == SIR_SCRIPT_INVALID_HANDLE );

	sirScriptOptimizationJobHandle_t hFirst = {};
	SIR_TEST_CHECK( coordinator.submit( first, true, &hFirst ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hFirst, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED );
	SIR_TEST_CHECK( coordinator.release( hFirst ) == SIR_SCRIPT_WRONG_STATE );
	sirScriptOptimizationPumpStats_t stats = {};
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( stats.nSent == 1 && stats.nCompleted == 1 && stats.nReceived == 1 );
	SIR_TEST_CHECK( transport.eReentrant == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( coordinator.query( hFirst, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_COMPLETED );
	SIR_TEST_CHECK( info.code.nSlot != 0 && info.entry.nSlot != 0 );
	const sirScriptCodeHandle_t codeFirst = info.code;
	const sirScriptCachedEntryHandle_t entryFirst = info.entry;
	SIR_TEST_CHECK( coordinator.cancel( hFirst ) == SIR_SCRIPT_WRONG_STATE );
	uint64_t nActive = 0;
	SIR_TEST_CHECK( manager.activeRevision( 1, 22, &nActive ) == SIR_SCRIPT_OK && nActive == 1 );
	SIR_TEST_CHECK( coordinator.release( hFirst ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hFirst, &info ) == SIR_SCRIPT_INVALID_HANDLE );
	SIR_TEST_CHECK( memory.nLive == 1 && memory.eReentrant == SIR_SCRIPT_BUSY );

	// An already loaded revision stays pinned while a new one is activated.
	const sirScriptProgramSourceAPI_t runtimeSource = manager.makeProgramSourceAPI();
	sirScriptProgramView_t view = {};
	uint64_t nOldLease = 0;
	SIR_TEST_CHECK( runtimeSource.pAcquire( runtimeSource.pUser, 1, 22, 1, &view, &nOldLease ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( nOldLease != 0 && view.nBytes == 256 && ( ( const uint8_t * )view.pData )[0] == 1 );
	sirScriptOptimizationJobHandle_t hSecond = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 2, registry.schemaFingerprint() ), true, &hSecond ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK && stats.nCompleted == 1 );
	SIR_TEST_CHECK( manager.activeRevision( 1, 22, &nActive ) == SIR_SCRIPT_OK && nActive == 2 );
	SIR_TEST_CHECK( ( ( const uint8_t * )view.pData )[0] == 1 );
	runtimeSource.pRelease( runtimeSource.pUser, nOldLease );
	SIR_TEST_CHECK( memory.nLive == 1 );
	SIR_TEST_CHECK( source.unregisterArtifact( entryFirst ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.release( hSecond ) == SIR_SCRIPT_OK );

	// Wrong binding schema forces provider acquisition rollback.
	sirScriptOptimizationJobHandle_t hMismatch = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 3, registry.schemaFingerprint() + 1 ), true, &hMismatch ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK && stats.nFailed == 1 );
	SIR_TEST_CHECK( coordinator.query( hMismatch, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED &&
	                info.eResult == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( source.stats().nRegisteredArtifacts == 1 );
	SIR_TEST_CHECK( coordinator.release( hMismatch ) == SIR_SCRIPT_OK );

	// Dispatched cancellation is a tombstone until the worker reply arrives.
	transport.bDeliver = false;
	sirScriptOptimizationJobHandle_t hCanceled = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 4, registry.schemaFingerprint() ), true, &hCanceled ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK && stats.nSent == 1 );
	SIR_TEST_CHECK( coordinator.cancel( hCanceled ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.release( hCanceled ) == SIR_SCRIPT_WRONG_STATE );
	transport.bDeliver = true;
	SIR_TEST_CHECK( coordinator.pump( 0, 1, &stats ) == SIR_SCRIPT_OK && stats.nIgnoredCanceled == 1 );
	SIR_TEST_CHECK( coordinator.release( hCanceled ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.stats().nRegisteredArtifacts == 1 );

	SIR_TEST_CHECK( coordinator.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.nStops == 1 );
	SIR_TEST_CHECK( manager.retire( codeFirst ) == SIR_SCRIPT_INVALID_HANDLE );
	// Find the currently active revision's code handle from the prior reply.
	// The code manager owns it independently of the completed job record.
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memory.nLive == 0 && memory.nAcquired == memory.nReleased );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) >= 1 );
	return true;
}

static bool testFailureAndCapacity( void )
{
	const std::filesystem::path root =
	    std::filesystem::temp_directory_path() /
	    ( "sir-optimizer-errors-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	SIR_TEST_CHECK( std::filesystem::create_directory( root ) );
	SIRScriptArtifactCache cache;
	SIR_TEST_CHECK( cache.configure( root.string().c_str(), 1024 ) == SIR_SCRIPT_OK );
	SIRScriptBindingRegistry registry;
	SIR_TEST_CHECK( registry.publish() == SIR_SCRIPT_OK );
	sirOptimizerTestMemory_t memory = {};
	sirScriptPayloadAllocatorAPI_t allocator = {
	    SIR_SCRIPT_PAYLOAD_ALLOCATOR_ABI_REVISION, &memory, 1024, 64, optimizerTestAllocate, optimizerTestFree };
	SIRScriptCachedProgramSource source;
	SIR_TEST_CHECK( source.configure( &cache, allocator, 1024 ) == SIR_SCRIPT_OK );
	SIRScriptCodeManager manager( &registry );
	SIR_TEST_CHECK( manager.configure( 88, source.makeProgramSourceAPI() ) == SIR_SCRIPT_OK );
	sirOptimizerTestTransport_t transport = {};
	transport.pCache = &cache;
	transport.bDeliver = false;
	SIRScriptOptimizationCoordinator coordinator;
	SIR_TEST_CHECK( coordinator.configure( optimizerTestAPI( &transport ), &source, &manager ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );

	// Saturation is deterministic and recovery requires releasing a terminal job.
	sirScriptOptimizationJobHandle_t handles[64] = {};
	for ( uint32_t i = 0; i < 64; ++i ) {
		SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( i + 1, registry.schemaFingerprint() ), false, &handles[i] ) ==
		                SIR_SCRIPT_OK );
	}
	sirScriptOptimizationJobHandle_t nExtra = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 65, registry.schemaFingerprint() ), false, &nExtra ) ==
	                SIR_SCRIPT_CAPACITY_EXCEEDED );
	for ( uint32_t i = 0; i < 64; ++i ) {
		SIR_TEST_CHECK( coordinator.cancel( handles[i] ) == SIR_SCRIPT_OK );
		SIR_TEST_CHECK( coordinator.release( handles[i] ) == SIR_SCRIPT_OK );
	}
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 65, registry.schemaFingerprint() ), false, &nExtra ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( handles[0], nullptr ) == SIR_SCRIPT_INVALID_ARGUMENT );
	SIR_TEST_CHECK( coordinator.cancel( handles[0] ) == SIR_SCRIPT_INVALID_HANDLE );
	sirScriptOptimizationPumpStats_t stats = {};
	transport.bCorrupt = true;
	transport.bDeliver = true;
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_TYPE_MISMATCH );
	sirScriptOptimizationJobInfo_t info = {};
	SIR_TEST_CHECK( coordinator.query( nExtra, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED );
	SIR_TEST_CHECK( coordinator.release( nExtra ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( transport.nStops == 1 );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	transport.bCorrupt = false;
	transport.bSendFail = true;
	sirScriptOptimizationJobHandle_t hSendFail = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 70, registry.schemaFingerprint() ), false, &hSendFail ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_BACKEND_FAILURE );
	transport.bSendFail = false;
	SIR_TEST_CHECK( coordinator.query( hSendFail, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	transport.bReceiveFail = true;
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( coordinator.query( hSendFail, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED );
	SIR_TEST_CHECK( coordinator.release( hSendFail ) == SIR_SCRIPT_OK );
	transport.bReceiveFail = false;
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	transport.bReturnBackendFailure = true;
	sirScriptOptimizationJobHandle_t hWorkerFailure = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 71, registry.schemaFingerprint() ), false, &hWorkerFailure ) ==
	                SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK && stats.nFailed == 1 );
	SIR_TEST_CHECK( coordinator.query( hWorkerFailure, &info ) == SIR_SCRIPT_OK && info.eResult == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( coordinator.release( hWorkerFailure ) == SIR_SCRIPT_OK );

	// Per-pump send budget is FIFO; an external watchdog can fail in-flight
	// work without losing queued jobs or retaining canceled tombstones.
	transport.bReturnBackendFailure = false;
	transport.bDeliver = false;
	sirScriptOptimizationJobHandle_t hInflight = {};
	sirScriptOptimizationJobHandle_t hPending = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 72, registry.schemaFingerprint() ), false, &hInflight ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 73, registry.schemaFingerprint() ), false, &hPending ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 0, &stats ) == SIR_SCRIPT_OK && stats.nSent == 1 );
	SIR_TEST_CHECK( coordinator.query( hInflight, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_IN_FLIGHT );
	SIR_TEST_CHECK( coordinator.query( hPending, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED );
	SIR_TEST_CHECK( coordinator.cancel( hInflight ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.workerFailed() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hInflight, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_CANCELED &&
	                !info.bAwaitingReply );
	SIR_TEST_CHECK( coordinator.release( hInflight ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.cancel( hPending ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.release( hPending ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );

	// A failed in-flight compile is explicitly reissued with a fresh wire ID.
	transport.bReturnBackendFailure = true;
	transport.bDeliver = false;
	sirScriptOptimizationJobHandle_t hRetry = {};
	SIR_TEST_CHECK( coordinator.submit( optimizerTestRequest( 74, registry.schemaFingerprint() ), false, &hRetry ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 0, &stats ) == SIR_SCRIPT_OK && stats.nSent == 1 );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_IN_FLIGHT );
	const uint64_t nOldJobId = info.nJobId;
	SIR_TEST_CHECK( coordinator.retryFailed( hRetry ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( coordinator.workerFailed() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED &&
	                info.eResult == SIR_SCRIPT_BACKEND_FAILURE && !info.bAwaitingReply );
	SIR_TEST_CHECK( coordinator.retryFailed( { 0, 0 } ) == SIR_SCRIPT_INVALID_HANDLE );
	sirScriptResult_t eRetryThread = SIR_SCRIPT_OK;
	std::thread retryThread( [&]() { eRetryThread = coordinator.retryFailed( hRetry ); } );
	retryThread.join();
	SIR_TEST_CHECK( eRetryThread == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( coordinator.retryFailed( hRetry ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_QUEUED &&
	                info.nJobId > nOldJobId && !info.bAwaitingReply );
	SIR_TEST_CHECK( coordinator.retryFailed( hRetry ) == SIR_SCRIPT_WRONG_STATE );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 0, &stats ) == SIR_SCRIPT_OK && stats.nSent == 1 );

	// Even a correctly encoded late reply from the old worker is rejected.
	sirScriptOptimizationReply_t staleReply = {};
	staleReply.nJobId = nOldJobId;
	staleReply.eResult = SIR_SCRIPT_BACKEND_FAILURE;
	SIR_TEST_CHECK( SIRScriptOptimizationProtocol::encodeReply( staleReply, transport.szPending[transport.nFront] ) == SIR_SCRIPT_OK );
	transport.bDeliver = true;
	SIR_TEST_CHECK( coordinator.pump( 0, 1, &stats ) == SIR_SCRIPT_TYPE_MISMATCH );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED );
	SIR_TEST_CHECK( coordinator.retryFailed( hRetry ) == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.nJobId > nOldJobId + 1 );
	SIR_TEST_CHECK( coordinator.start() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( coordinator.pump( 1, 1, &stats ) == SIR_SCRIPT_OK && stats.nFailed == 1 );
	SIR_TEST_CHECK( coordinator.query( hRetry, &info ) == SIR_SCRIPT_OK && info.eState == SIR_SCRIPT_JOB_FAILED &&
	                info.eResult == SIR_SCRIPT_BACKEND_FAILURE );
	SIR_TEST_CHECK( coordinator.release( hRetry ) == SIR_SCRIPT_OK );

	sirScriptOptimizationJobHandle_t hThread = {};
	sirScriptResult_t eWrongThread = SIR_SCRIPT_OK;
	std::thread thread(
	    [&]() { eWrongThread = coordinator.submit( optimizerTestRequest( 72, registry.schemaFingerprint() ), false, &hThread ); } );
	thread.join();
	SIR_TEST_CHECK( eWrongThread == SIR_SCRIPT_BUSY );
	SIR_TEST_CHECK( coordinator.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( source.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( manager.shutdown() == SIR_SCRIPT_OK );
	SIR_TEST_CHECK( memory.nLive == 0 );
	SIR_TEST_CHECK( std::filesystem::remove_all( root ) >= 1 );
	return true;
}

int main( void )
{
	if ( !testLifecycleAndRollback() || !testFailureAndCapacity() ) {
		return 1;
	}
	puts( "scripting optimization coordinator: PASS" );
	return 0;
}

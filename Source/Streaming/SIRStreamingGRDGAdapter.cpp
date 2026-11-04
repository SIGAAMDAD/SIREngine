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

#include <limits.h>
#include <new>
#include <stdlib.h>
#include <string.h>

typedef enum sirStreamingGRDGSlotState_e : uint8_t
{
	SIR_STREAMING_GRDG_FREE = 0,
	SIR_STREAMING_GRDG_WAITING = 1,
	SIR_STREAMING_GRDG_LOADING = 2,
	SIR_STREAMING_GRDG_PIN_PENDING = 3,
	SIR_STREAMING_GRDG_FAILED_PENDING = 4,
	SIR_STREAMING_GRDG_PREPARED = 5,
	SIR_STREAMING_GRDG_FAILED = 6
} sirStreamingGRDGSlotState_t;

typedef enum sirStreamingGRDGProbe_e : uint8_t
{
	SIR_STREAMING_GRDG_PROBE_MISSING = 0,
	SIR_STREAMING_GRDG_PROBE_PINNED = 1,
	SIR_STREAMING_GRDG_PROBE_RETRY = 2,
	SIR_STREAMING_GRDG_PROBE_MISMATCH = 3
} sirStreamingGRDGProbe_t;

typedef struct sirStreamingGRDGSlot_s
{
	SIRResourceFileLoad load;
	const sirStreamingGRDGBinding_t *pBinding;
	sirStreamingTicket_t ticket;
	sirStreamingGRDGResident_t resident;
	SIRResourcePinHandle hPin;
	sirStreamingGRDGSlotState_t eState;
	bool bCancelRequested;
	bool bHasPin;

	sirStreamingGRDGSlot_s( SIRResourceGraph& rGraph, SIRTaskScheduler& rScheduler ) noexcept
	    : load( rGraph, rScheduler ), pBinding( nullptr ), ticket{}, resident{}, hPin{}, eState( SIR_STREAMING_GRDG_FREE ),
	      bCancelRequested( false ), bHasPin( false )
	{
	}
} sirStreamingGRDGSlot_t;

static int streamingGRDGCompareKeys( const sirStreamingUnitKey_t& a, const sirStreamingUnitKey_t& b ) noexcept
{
	if ( a.nContainerId != b.nContainerId ) {
		return a.nContainerId < b.nContainerId ? -1 : 1;
	}
	if ( a.nUnitId != b.nUnitId ) {
		return a.nUnitId < b.nUnitId ? -1 : 1;
	}
	if ( a.nManifestGeneration != b.nManifestGeneration ) {
		return a.nManifestGeneration < b.nManifestGeneration ? -1 : 1;
	}
	if ( a.nVariant != b.nVariant ) {
		return a.nVariant < b.nVariant ? -1 : 1;
	}
	if ( a.eKind != b.eKind ) {
		return a.eKind < b.eKind ? -1 : 1;
	}
	return 0;
}

static bool streamingGRDGResourcesEqual( const SIRResourceKey& a, const SIRResourceKey& b ) noexcept
{
	return a.m_nAssetId == b.m_nAssetId && a.m_nTypeId == b.m_nTypeId && a.m_nVariantId == b.m_nVariantId;
}

static bool streamingGRDGContentEqual( const SIRResourceContentKey& a, const SIRResourceContentKey& b ) noexcept
{
	return a.m_nHashLo == b.m_nHashLo && a.m_nHashHi == b.m_nHashHi && a.m_nFormatId == b.m_nFormatId && a.m_nEncodingId == b.m_nEncodingId;
}

// Multiple logical units may share one canonical GRDG resource only when
// their complete load contracts agree. Otherwise the first request could
// publish a payload that satisfies one caller but invalidates another.
static bool streamingGRDGDependenciesEqual( const sirResourceFileLoadRequest_t& a, const sirResourceFileLoadRequest_t& b ) noexcept
{
	if ( a.nDependencyCount != b.nDependencyCount ) {
		return false;
	}
	for ( uint32_t iEdge = 0; iEdge < a.nDependencyCount; ++iEdge ) {
		const SIRResourceDependency& left = a.pDependencies[iEdge];
		const SIRResourceDependency& right = b.pDependencies[iEdge];
		if ( !streamingGRDGResourcesEqual( left.m_Target, right.m_Target ) || left.m_eKind != right.m_eKind ) {
			return false;
		}
	}
	return true;
}

static bool streamingGRDGLoadContractsEqual( const sirStreamingGRDGBinding_t& a, const sirStreamingGRDGBinding_t& b ) noexcept
{
	const sirResourceFileLoadRequest_t& left = a.request;
	const sirResourceFileLoadRequest_t& right = b.request;
	if ( strcmp( a.pszFilePath, b.pszFilePath ) != 0 || !streamingGRDGContentEqual( left.keyContent, right.keyContent ) ) {
		return false;
	}
	if ( left.idChunk.m_nValue != right.idChunk.m_nValue || left.eKind != right.eKind || left.eMode != right.eMode ) {
		return false;
	}
	const bool bFormatEqual = left.nSchemaVersion == right.nSchemaVersion && left.nFormatId == right.nFormatId;
	const bool bAllocationEqual = left.nAlignment == right.nAlignment && left.nStagingDomain == right.nStagingDomain;
	const bool bRangeEqual = left.nFileOffset == right.nFileOffset && left.nFileBytes == right.nFileBytes;
	if ( !bFormatEqual || !bAllocationEqual || !bRangeEqual || left.nExpectedRevision != right.nExpectedRevision ) {
		return false;
	}
	return streamingGRDGDependenciesEqual( left, right );
}

SIRStreamingGRDGAdapter::SIRStreamingGRDGAdapter( void ) noexcept
    : m_pGraph( nullptr ), m_pScheduler( nullptr ), m_pCoordinator( nullptr ), m_pBridge( nullptr ), m_pBindings( nullptr ),
      m_pSlots( nullptr ), m_Callbacks{}, m_nBindingCount( 0 ), m_nCapacity( 0 ), m_nProviderId( 0 ), m_nMaxConcurrentLoads( 0 ),
      m_nActiveLoads( 0 ), m_iNextSlot( 0 ), m_bInitialized( false )
{
}

SIRStreamingGRDGAdapter::~SIRStreamingGRDGAdapter( void ) noexcept
{
	// A live transfer or provider pin must be drained by shutdown before this
	// object and its caller-owned memory are destroyed.
	if ( m_bInitialized ) {
		abort();
	}
}

uint64_t SIRStreamingGRDGAdapter::requiredBytes( uint32_t nCapacity ) noexcept
{
	return ( uint64_t )nCapacity * sizeof( sirStreamingGRDGSlot_t );
}

sirStreamingResult_t SIRStreamingGRDGAdapter::initialize( const sirStreamingGRDGConfig_t& config ) noexcept
{
	if ( m_bInitialized || !config.pGraph || !config.pScheduler || !config.pCoordinator || !config.pStorage ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( !config.pGraph->isReady() || !config.pGraph->getWriter() || !config.pBindings || !config.nBindingCount ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( !config.nProviderId || !config.nCapacity || !config.callbacks.pBeginRetire || !config.callbacks.pBeginActivate ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.nMaxConcurrentLoads > config.nCapacity ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	const uint64_t nRequired = requiredBytes( config.nCapacity );
	if ( nRequired > SIZE_MAX || nRequired > config.nStorageBytes ||
	     ( ( uintptr_t )config.pStorage % alignof( sirStreamingGRDGSlot_t ) ) != 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( config.pCoordinator->getStats().nCapacity != config.nCapacity || config.pCoordinator->getStats().nOccupied != 0 ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	const uintptr_t nStorageStart = ( uintptr_t )config.pStorage;
	const uintptr_t nBindingsStart = ( uintptr_t )config.pBindings;
	const uint64_t nBindingBytes = ( uint64_t )config.nBindingCount * sizeof( sirStreamingGRDGBinding_t );
	if ( nRequired > UINTPTR_MAX - nStorageStart || nBindingBytes > UINTPTR_MAX - nBindingsStart ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( nStorageStart < nBindingsStart + nBindingBytes && nBindingsStart < nStorageStart + nRequired ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	for ( uint32_t iBinding = 0; iBinding < config.nBindingCount; ++iBinding ) {
		const sirStreamingGRDGBinding_t& entry = config.pBindings[iBinding];
		if ( entry.keyUnit.nUnitId == 0 || entry.keyUnit.nManifestGeneration == 0 || entry.keyUnit.eKind >= SIR_STREAMING_UNIT_COUNT ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( !entry.pszFilePath || !entry.pszFilePath[0] || !entry.request.keyResource.m_nTypeId || !entry.request.nFileBytes ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( !entry.request.nAlignment || ( entry.request.nAlignment & ( entry.request.nAlignment - 1 ) ) != 0 ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( entry.request.nDependencyCount && !entry.request.pDependencies ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		if ( iBinding != 0 && streamingGRDGCompareKeys( config.pBindings[iBinding - 1].keyUnit, entry.keyUnit ) >= 0 ) {
			return SIR_STREAMING_INVALID_ARGUMENT;
		}
		for ( uint32_t iPrevious = 0; iPrevious < iBinding; ++iPrevious ) {
			const sirStreamingGRDGBinding_t& previous = config.pBindings[iPrevious];
			if ( !streamingGRDGResourcesEqual( entry.request.keyResource, previous.request.keyResource ) ) {
				continue;
			}
			if ( !streamingGRDGLoadContractsEqual( entry, previous ) ) {
				return SIR_STREAMING_INVALID_ARGUMENT;
			}
		}
	}

	m_pGraph = config.pGraph;
	m_pScheduler = config.pScheduler;
	m_pCoordinator = config.pCoordinator;
	m_pSlots = ( sirStreamingGRDGSlot_t * )config.pStorage;
	m_pBindings = config.pBindings;
	m_Callbacks = config.callbacks;
	m_nBindingCount = config.nBindingCount;
	m_nCapacity = config.nCapacity;
	m_nProviderId = config.nProviderId;
	m_nMaxConcurrentLoads = config.nMaxConcurrentLoads ? config.nMaxConcurrentLoads : config.nCapacity;
	m_nActiveLoads = 0;
	m_iNextSlot = 0;
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		new ( m_pSlots + iSlot ) sirStreamingGRDGSlot_t( *m_pGraph, *m_pScheduler );
	}
	m_bInitialized = true;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingGRDGAdapter::bindBridge( SIRStreamingProviderBridge *pBridge ) noexcept
{
	if ( !m_bInitialized || m_pBridge || !pBridge ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( m_pCoordinator->getStats().nOccupied != 0 ) {
		return SIR_STREAMING_BUSY;
	}
	m_pBridge = pBridge;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingGRDGAdapter::shutdown( void ) noexcept
{
	if ( !m_bInitialized ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( m_pCoordinator->getStats().nOccupied != 0 ) {
		return SIR_STREAMING_BUSY;
	}
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		if ( m_pSlots[iSlot].eState != SIR_STREAMING_GRDG_FREE || m_pSlots[iSlot].load.isActive() ) {
			return SIR_STREAMING_BUSY;
		}
	}

	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		m_pSlots[iSlot].~sirStreamingGRDGSlot_s();
	}
	m_pGraph = nullptr;
	m_pScheduler = nullptr;
	m_pCoordinator = nullptr;
	m_pBridge = nullptr;
	m_pBindings = nullptr;
	m_pSlots = nullptr;
	m_Callbacks = {};
	m_nBindingCount = 0;
	m_nCapacity = 0;
	m_nProviderId = 0;
	m_nMaxConcurrentLoads = 0;
	m_nActiveLoads = 0;
	m_bInitialized = false;
	return SIR_STREAMING_OK;
}

sirStreamingProviderCallbacks_t SIRStreamingGRDGAdapter::getBridgeCallbacks( void ) noexcept
{
	sirStreamingProviderCallbacks_t callbacks = {};
	callbacks.pContext = this;
	callbacks.pBeginPrepare = &SIRStreamingGRDGAdapter::beginPrepareCallback;
	callbacks.pCancelPrepare = &SIRStreamingGRDGAdapter::cancelPrepareCallback;
	callbacks.pBeginActivate = &SIRStreamingGRDGAdapter::beginActivateCallback;
	callbacks.pBeginRetire = &SIRStreamingGRDGAdapter::beginRetireCallback;
	return callbacks;
}

const sirStreamingGRDGBinding_t *SIRStreamingGRDGAdapter::findBinding( const sirStreamingUnitKey_t& key ) const noexcept
{
	uint32_t iFirst = 0;
	uint32_t iLast = m_nBindingCount;
	while ( iFirst < iLast ) {
		const uint32_t iMiddle = iFirst + ( iLast - iFirst ) / 2;
		const int iCompare = streamingGRDGCompareKeys( key, m_pBindings[iMiddle].keyUnit );
		if ( iCompare == 0 ) {
			return &m_pBindings[iMiddle];
		}
		if ( iCompare > 0 ) {
			iFirst = iMiddle + 1;
		} else {
			iLast = iMiddle;
		}
	}
	return nullptr;
}

bool SIRStreamingGRDGAdapter::isOtherLoadActive( uint32_t iExclude, const SIRResourceKey& key ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_nCapacity; ++iSlot ) {
		const sirStreamingGRDGSlot_t& slot = m_pSlots[iSlot];
		if ( iSlot == iExclude || slot.eState != SIR_STREAMING_GRDG_LOADING || !slot.pBinding ) {
			continue;
		}
		if ( streamingGRDGResourcesEqual( slot.pBinding->request.keyResource, key ) ) {
			return true;
		}
	}
	return false;
}

bool SIRStreamingGRDGAdapter::isCurrent( sirStreamingTicket_t ticket ) const noexcept
{
	if ( !m_bInitialized || !m_pBridge || ticket.nSlot >= m_nCapacity || ticket.nGeneration == 0 ) {
		return false;
	}
	const sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	return slot.eState != SIR_STREAMING_GRDG_FREE && slot.ticket.nGeneration == ticket.nGeneration;
}

bool SIRStreamingGRDGAdapter::beginPrepare( sirStreamingTicket_t ticket, const sirStreamingDemand_t *pDemand ) noexcept
{
	if ( !m_bInitialized || !m_pBridge || !pDemand || ticket.nSlot >= m_nCapacity || !ticket.nGeneration ) {
		return false;
	}
	sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.eState != SIR_STREAMING_GRDG_FREE ) {
		return false;
	}

	slot.pBinding = findBinding( pDemand->keyUnit );
	slot.ticket = ticket;
	slot.resident = {};
	slot.hPin = {};
	slot.bHasPin = false;
	slot.bCancelRequested = false;
	slot.eState = slot.pBinding ? SIR_STREAMING_GRDG_WAITING : SIR_STREAMING_GRDG_FAILED_PENDING;
	return true;
}

bool SIRStreamingGRDGAdapter::cancelPrepare( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isCurrent( ticket ) ) {
		return false;
	}
	sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	slot.bCancelRequested = true;
	if ( slot.load.isActive() ) {
		return slot.load.cancel() == SIR_RESOURCE_RESULT_SUCCESS;
	}
	if ( slot.eState == SIR_STREAMING_GRDG_WAITING ) {
		slot.eState = SIR_STREAMING_GRDG_FAILED_PENDING;
	}
	return true;
}

bool SIRStreamingGRDGAdapter::beginActivate( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isCurrent( ticket ) ) {
		return false;
	}
	const sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.eState != SIR_STREAMING_GRDG_PREPARED || !slot.bHasPin ) {
		return false;
	}
	return m_Callbacks.pBeginActivate( m_Callbacks.pContext, ticket, &slot.resident );
}

bool SIRStreamingGRDGAdapter::beginRetire( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isCurrent( ticket ) ) {
		return false;
	}
	const sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.load.isActive() ) {
		return false;
	}
	return m_Callbacks.pBeginRetire( m_Callbacks.pContext, ticket, &slot.resident );
}

bool SIRStreamingGRDGAdapter::beginPrepareCallback( void *pContext, sirStreamingTicket_t ticket,
                                                    const sirStreamingDemand_t *pDemand ) noexcept
{
	return pContext && ( ( SIRStreamingGRDGAdapter * )pContext )->beginPrepare( ticket, pDemand );
}

bool SIRStreamingGRDGAdapter::cancelPrepareCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	return pContext && ( ( SIRStreamingGRDGAdapter * )pContext )->cancelPrepare( ticket );
}

bool SIRStreamingGRDGAdapter::beginActivateCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	return pContext && ( ( SIRStreamingGRDGAdapter * )pContext )->beginActivate( ticket );
}

bool SIRStreamingGRDGAdapter::beginRetireCallback( void *pContext, sirStreamingTicket_t ticket ) noexcept
{
	return pContext && ( ( SIRStreamingGRDGAdapter * )pContext )->beginRetire( ticket );
}

SIRResourceResult SIRStreamingGRDGAdapter::startLoad( sirStreamingGRDGSlot_t& slot ) noexcept
{
	if ( !slot.pBinding || slot.load.isActive() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( m_nActiveLoads >= m_nMaxConcurrentLoads ) {
		return SIR_RESOURCE_RESULT_BUSY;
	}
	SIRResourceHandle hRequest = {};
	SIRResourceGraphWriter *pWriter = m_pGraph->getWriter();
	if ( !pWriter ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	const SIRResourceResult eRequest = pWriter->request( slot.pBinding->request.keyResource, &hRequest );
	if ( eRequest != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eRequest;
	}
	const SIRResourceResult eResult = slot.load.begin( slot.pBinding->pszFilePath, slot.pBinding->request );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	slot.eState = SIR_STREAMING_GRDG_LOADING;
	++m_nActiveLoads;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

uint8_t SIRStreamingGRDGAdapter::probeResident( sirStreamingGRDGSlot_t& slot ) noexcept
{
	SIRResourceReadScope scope( *m_pGraph );
	if ( !scope.isActive() ) {
		return SIR_STREAMING_GRDG_PROBE_RETRY;
	}

	const sirResourceFileLoadRequest_t& request = slot.pBinding->request;
	SIRResourceHandle hResource = {};
	const SIRResourceResult eFound = m_pGraph->find( scope, request.keyResource, &hResource );
	if ( eFound == SIR_RESOURCE_RESULT_INVALID_HANDLE ) {
		return SIR_STREAMING_GRDG_PROBE_MISSING;
	}
	if ( eFound != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_STREAMING_GRDG_PROBE_RETRY;
	}

	SIRResourceView view;
	const SIRResourceResult eResolved = m_pGraph->resolve( scope, hResource, &view );
	if ( eResolved == SIR_RESOURCE_RESULT_NOT_RESIDENT ) {
		return SIR_STREAMING_GRDG_PROBE_MISSING;
	}
	if ( eResolved != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_STREAMING_GRDG_PROBE_RETRY;
	}

	SIRResourceInfo info = {};
	if ( view.getInfo( &info ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_STREAMING_GRDG_PROBE_MISMATCH;
	}
	if ( info.m_eKind != request.eKind || info.m_nSchemaVersion != request.nSchemaVersion || info.m_nFormatId != request.nFormatId ) {
		return SIR_STREAMING_GRDG_PROBE_MISMATCH;
	}

	bool bMatchingChunk = false;
	for ( uint32_t iChunk = 0; iChunk < info.m_nChunkCount; ++iChunk ) {
		SIRResourceChunkInfo chunk = {};
		if ( view.getChunkInfo( iChunk, &chunk ) != SIR_RESOURCE_RESULT_SUCCESS ) {
			return SIR_STREAMING_GRDG_PROBE_MISMATCH;
		}
		if ( chunk.m_Id.m_nValue == request.idChunk.m_nValue ) {
			bMatchingChunk = streamingGRDGContentEqual( chunk.m_ContentKey, request.keyContent );
			bMatchingChunk = bMatchingChunk && chunk.m_nBytes == request.nFileBytes;
			break;
		}
	}
	if ( !bMatchingChunk ) {
		return SIR_STREAMING_GRDG_PROBE_MISMATCH;
	}

	SIRResourcePinHandle hPin = {};
	const SIRResourceResult ePin = m_pGraph->pinForProvider( scope, view, m_nProviderId, &hPin );
	if ( ePin == SIR_RESOURCE_RESULT_OUT_OF_BUDGET ) {
		return SIR_STREAMING_GRDG_PROBE_RETRY;
	}
	if ( ePin != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_STREAMING_GRDG_PROBE_MISMATCH;
	}
	slot.resident = { hResource, view.getRevision() };
	slot.hPin = hPin;
	slot.bHasPin = true;
	return SIR_STREAMING_GRDG_PROBE_PINNED;
}

bool SIRStreamingGRDGAdapter::checkPublishedRevision( sirStreamingGRDGSlot_t& slot ) noexcept
{
	SIRResourceReadScope scope( *m_pGraph );
	if ( !scope.isActive() ) {
		return true;
	}
	SIRResourceView view;
	if ( m_pGraph->resolve( scope, slot.resident.hResource, &view ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		m_pCoordinator->cancel( slot.ticket );
		return false;
	}
	if ( view.getRevision() != slot.resident.nRevision ) {
		m_pCoordinator->cancel( slot.ticket );
		return false;
	}
	return true;
}

void SIRStreamingGRDGAdapter::processSlot( uint32_t iSlot, sirStreamingGRDGStats_t& stats ) noexcept
{
	sirStreamingGRDGSlot_t& slot = m_pSlots[iSlot];
	if ( slot.eState == SIR_STREAMING_GRDG_FREE || slot.eState == SIR_STREAMING_GRDG_FAILED ) {
		return;
	}
	if ( slot.eState == SIR_STREAMING_GRDG_PREPARED ) {
		if ( slot.bHasPin ) {
			checkPublishedRevision( slot );
		}
		return;
	}
	if ( slot.bCancelRequested && !slot.load.isActive() && slot.eState != SIR_STREAMING_GRDG_FAILED_PENDING ) {
		slot.eState = SIR_STREAMING_GRDG_FAILED_PENDING;
	}

	if ( slot.eState == SIR_STREAMING_GRDG_LOADING ) {
		SIRResourceHandle hPublished = {};
		const SIRResourceResult eResult = slot.load.poll( &hPublished );
		if ( eResult == SIR_RESOURCE_RESULT_BUSY || slot.load.isActive() ) {
			++stats.nWaiting;
			return;
		}
		--m_nActiveLoads;
		slot.eState = eResult == SIR_RESOURCE_RESULT_SUCCESS || eResult == SIR_RESOURCE_RESULT_STALE_REVISION
		                  ? SIR_STREAMING_GRDG_PIN_PENDING
		                  : SIR_STREAMING_GRDG_FAILED_PENDING;
	}

	if ( slot.eState == SIR_STREAMING_GRDG_WAITING || slot.eState == SIR_STREAMING_GRDG_PIN_PENDING ) {
		const uint8_t eProbe = probeResident( slot );
		if ( eProbe == SIR_STREAMING_GRDG_PROBE_PINNED ) {
			slot.eState = SIR_STREAMING_GRDG_PIN_PENDING;
		} else if ( eProbe == SIR_STREAMING_GRDG_PROBE_MISMATCH ) {
			slot.eState = SIR_STREAMING_GRDG_FAILED_PENDING;
		} else if ( eProbe == SIR_STREAMING_GRDG_PROBE_MISSING && slot.eState == SIR_STREAMING_GRDG_WAITING ) {
			if ( isOtherLoadActive( iSlot, slot.pBinding->request.keyResource ) ) {
				++stats.nWaiting;
				return;
			}
			const SIRResourceResult eStarted = startLoad( slot );
			if ( eStarted == SIR_RESOURCE_RESULT_SUCCESS ) {
				++stats.nStarted;
				return;
			}
			if ( eStarted == SIR_RESOURCE_RESULT_OUT_OF_BUDGET || eStarted == SIR_RESOURCE_RESULT_BUSY ) {
				++stats.nWaiting;
				return;
			}
			slot.eState = SIR_STREAMING_GRDG_FAILED_PENDING;
		} else {
			++stats.nWaiting;
			return;
		}
	}

	if ( slot.eState == SIR_STREAMING_GRDG_PIN_PENDING ) {
		if ( !slot.bHasPin ) {
			++stats.nWaiting;
			return;
		}
		if ( m_pBridge->acknowledgePrepared( slot.ticket, true ) == SIR_STREAMING_OK ) {
			slot.eState = SIR_STREAMING_GRDG_PREPARED;
			++stats.nPrepared;
		}
		return;
	}
	if ( slot.eState == SIR_STREAMING_GRDG_FAILED_PENDING ) {
		if ( m_pBridge->acknowledgePrepared( slot.ticket, false ) == SIR_STREAMING_OK ) {
			slot.eState = SIR_STREAMING_GRDG_FAILED;
			++stats.nFailed;
		}
	}
}

sirStreamingResult_t SIRStreamingGRDGAdapter::pump( uint32_t nMaximumSlots, sirStreamingGRDGStats_t *pStats ) noexcept
{
	if ( !m_bInitialized || !m_pBridge ) {
		return SIR_STREAMING_NOT_READY;
	}
	if ( !nMaximumSlots || !pStats ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}

	sirStreamingGRDGStats_t stats = {};
	const uint32_t nVisits = nMaximumSlots < m_nCapacity ? nMaximumSlots : m_nCapacity;
	for ( uint32_t iVisit = 0; iVisit < nVisits; ++iVisit ) {
		processSlot( m_iNextSlot, stats );
		m_iNextSlot = m_iNextSlot + 1 == m_nCapacity ? 0 : m_iNextSlot + 1;
		++stats.nVisited;
	}
	stats.nActiveLoads = m_nActiveLoads;
	stats.nMaxConcurrentLoads = m_nMaxConcurrentLoads;
	*pStats = stats;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingGRDGAdapter::queryResource( sirStreamingTicket_t ticket,
                                                             sirStreamingGRDGResident_t *pResident ) const noexcept
{
	if ( !pResident ) {
		return SIR_STREAMING_INVALID_ARGUMENT;
	}
	if ( !isCurrent( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}
	const sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.eState != SIR_STREAMING_GRDG_PREPARED || !slot.bHasPin ) {
		return SIR_STREAMING_NOT_READY;
	}
	*pResident = slot.resident;
	return SIR_STREAMING_OK;
}

sirStreamingResult_t SIRStreamingGRDGAdapter::acknowledgeActivated( sirStreamingTicket_t ticket, bool bSucceeded ) noexcept
{
	if ( !isCurrent( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}
	if ( m_pSlots[ticket.nSlot].eState != SIR_STREAMING_GRDG_PREPARED ) {
		return SIR_STREAMING_INVALID_TRANSITION;
	}
	return m_pBridge->acknowledgeActivated( ticket, bSucceeded );
}

sirStreamingResult_t SIRStreamingGRDGAdapter::acknowledgeRetired( sirStreamingTicket_t ticket ) noexcept
{
	if ( !isCurrent( ticket ) ) {
		return SIR_STREAMING_STALE_TICKET;
	}
	sirStreamingGRDGSlot_t& slot = m_pSlots[ticket.nSlot];
	if ( slot.load.isActive() ) {
		return SIR_STREAMING_BUSY;
	}

	sirStreamingSnapshot_t snapshot = {};
	if ( m_pCoordinator->query( ticket, &snapshot ) != SIR_STREAMING_OK || snapshot.eStage != SIR_STREAMING_STAGE_RETIRING ) {
		return SIR_STREAMING_INVALID_TRANSITION;
	}
	if ( slot.bHasPin && m_pGraph->releaseProviderPin( slot.hPin ) != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_STREAMING_BUSY;
	}
	slot.bHasPin = false;
	const sirStreamingResult_t eResult = m_pBridge->acknowledgeRetired( ticket );
	if ( eResult == SIR_STREAMING_OK ) {
		slot.pBinding = nullptr;
		slot.ticket = {};
		slot.resident = {};
		slot.hPin = {};
		slot.eState = SIR_STREAMING_GRDG_FREE;
		slot.bCancelRequested = false;
	}
	return eResult;
}

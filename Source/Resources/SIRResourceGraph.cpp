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

#include <Resources/SIRResourceGraph.h>

#include <limits.h>
#include <stdint.h>
#include <new>
#include <stdlib.h>
#include <string.h>

// Internal publication records and all CPU payloads belong to SIRResourceGraph.
// A record contains borrowed chunk pointers; the graph owns shared chunks.
struct SIRSharedResourceChunk
{
	SIRResourceContentKey m_ContentKey;
	uint8_t *m_pData;
	uint64_t m_nSize;
	uint32_t m_nAlignment;
	uint32_t m_nDomain;
	uint32_t m_nReferences;
	SIRSharedResourceChunk *m_pNext;
};

struct SIRRecordChunk
{
	SIRResourceChunkInfo m_Info;
	SIRSharedResourceChunk *m_pShared;
	uint32_t m_iStagingSlot;
	bool m_bNewShared;
};

struct SIRPublishedResourceRecord
{
	SIRResourceInfo m_Info;
	SIRResourceHandle m_Handle;
	SIRRecordChunk *m_pChunks;
	SIRResourceDependency *m_pDependencies;
	SIRPublishedResourceRecord *m_pRetiredNext;
	uint64_t m_nAllocationBytes;
	uint32_t m_nProviderPins; // Protected by the graph's pin mutex.
};

struct SIRGraphSlot
{
	std::atomic<SIRPublishedResourceRecord *> m_pPublished;
	uint32_t m_nNextGeneration;
	uint32_t m_nVisitedStamp;

	SIRGraphSlot( void ) noexcept : m_pPublished( nullptr ), m_nNextGeneration( 1 ), m_nVisitedStamp( 0 )
	{
	}
};

struct SIRStagingSlot
{
	uint8_t *m_pData;
	uint64_t m_nSize;
	uint32_t m_nAlignment;
	uint32_t m_nDomain;
	uint32_t m_nGeneration;
};

struct SIRProviderPinSlot
{
	SIRPublishedResourceRecord *m_pRecord;
	uint32_t m_nGeneration;
	uint32_t m_nProviderId;
};

static bool sameKey( const SIRResourceKey &rA, const SIRResourceKey &rB ) noexcept
{
	return rA.m_nAssetId == rB.m_nAssetId && rA.m_nTypeId == rB.m_nTypeId && rA.m_nVariantId == rB.m_nVariantId;
}

static bool sameContentKey( const SIRResourceContentKey &rA, const SIRResourceContentKey &rB ) noexcept
{
	return rA.m_nHashLo == rB.m_nHashLo && rA.m_nHashHi == rB.m_nHashHi && rA.m_nFormatId == rB.m_nFormatId &&
	       rA.m_nEncodingId == rB.m_nEncodingId;
}

static bool isPowerOfTwo( uint32_t nValue ) noexcept
{
	return nValue != 0 && ( nValue & ( nValue - 1 ) ) == 0;
}

static uint32_t nextGeneration( uint32_t nGeneration ) noexcept
{
	return nGeneration == UINT32_MAX ? 1 : nGeneration + 1;
}

static bool validHandle( SIRResourceHandle hA, SIRResourceHandle hB ) noexcept
{
	return hA.m_nSlot == hB.m_nSlot && hA.m_nGeneration == hB.m_nGeneration && hA.m_nTypeId == hB.m_nTypeId;
}

SIRResourceGraphWriter::SIRResourceGraphWriter( SIRResourceGraph &rGraph ) noexcept : m_pGraph( &rGraph )
{
}

SIRResourceGraph::SIRResourceGraph( const SIRResourceGraphConfig &rConfig ) noexcept
    : m_Config( rConfig ), m_Writer( *this ), m_OwnerThread( std::this_thread::get_id() ), m_nActiveReaders( 0 ),
      m_KeyIndexAllocator{ this, allocateKeyIndex, releaseKeyIndex, rConfig.m_nMetadataDomain, 0x47524447U },
      m_KeyIndex( &m_KeyIndexAllocator ), m_pSlots( nullptr ), m_pStaging( nullptr ), m_pProviderPins( nullptr ),
      m_pTraversalQueue( nullptr ), m_nTraversalStamp( 0 ), m_pSharedChunks( nullptr ), m_pRetired( nullptr ), m_nCommittedBytes( 0 ),
      m_nPeakCommittedBytes( 0 ), m_nPayloadBytes( 0 ), m_nUniqueChunks( 0 ), m_nRetiredRecords( 0 ), m_bReady( false )
{
	if ( !m_Config.m_Allocator.m_pfnAllocate || !m_Config.m_Allocator.m_pfnFree || !m_Config.m_nMaxResources || !m_Config.m_nMaxStaging ||
	     !m_Config.m_nMaxProviderPins || m_Config.m_nMaxResources > INT32_MAX ) {
		return;
	}
	const uint64_t nSlots = ( uint64_t )m_Config.m_nMaxResources * sizeof( SIRGraphSlot );
	const uint64_t nStaging = ( uint64_t )m_Config.m_nMaxStaging * sizeof( SIRStagingSlot );
	const uint64_t nPins = ( uint64_t )m_Config.m_nMaxProviderPins * sizeof( SIRProviderPinSlot );
	m_pSlots = ( SIRGraphSlot * )allocate( nSlots, alignof( SIRGraphSlot ), m_Config.m_nMetadataDomain );
	m_pStaging = ( SIRStagingSlot * )allocate( nStaging, alignof( SIRStagingSlot ), m_Config.m_nMetadataDomain );
	m_pProviderPins = ( SIRProviderPinSlot * )allocate( nPins, alignof( SIRProviderPinSlot ), m_Config.m_nMetadataDomain );
	m_pTraversalQueue = ( uint32_t * )allocate( ( uint64_t )m_Config.m_nMaxResources * sizeof( uint32_t ), alignof( uint32_t ),
	                                            m_Config.m_nMetadataDomain );
	if ( !m_pSlots || !m_pStaging || !m_pProviderPins || !m_pTraversalQueue ) {
		return;
	}
	for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxResources; ++iSlot ) {
		new ( &m_pSlots[iSlot] ) SIRGraphSlot();
	}
	memset( m_pStaging, 0, ( size_t )nStaging );
	memset( m_pProviderPins, 0, ( size_t )nPins );
	if ( m_KeyIndex.tryReserve( m_Config.m_nMaxResources ) != SIR_CONTAINER_OK ) {
		return;
	}
	m_bReady = true;
}

SIRResourceGraph::~SIRResourceGraph( void ) noexcept
{
	// Destruction with live scopes or pins is a caller lifetime violation.
	if ( m_nActiveReaders.load( std::memory_order_seq_cst ) != 0 ) {
		abort();
	}
	if ( m_pProviderPins ) {
		for ( uint32_t iPin = 0; iPin < m_Config.m_nMaxProviderPins; ++iPin ) {
			if ( m_pProviderPins[iPin].m_pRecord ) {
				abort();
			}
		}
	}
	if ( m_pSlots && m_bReady ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxResources; ++iSlot ) {
			SIRPublishedResourceRecord *pRecord = m_pSlots[iSlot].m_pPublished.load( std::memory_order_seq_cst );
			if ( pRecord ) {
				destroyRecord( pRecord );
			}
			m_pSlots[iSlot].~SIRGraphSlot();
		}
	}
	while ( m_pRetired ) {
		SIRPublishedResourceRecord *pRecord = m_pRetired;
		m_pRetired = pRecord->m_pRetiredNext;
		destroyRecord( pRecord );
	}
	if ( m_pStaging ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxStaging; ++iSlot ) {
			releaseStagingSlot( iSlot );
		}
	}
	release( m_pTraversalQueue, ( uint64_t )m_Config.m_nMaxResources * sizeof( uint32_t ), alignof( uint32_t ),
	         m_Config.m_nMetadataDomain );
	release( m_pProviderPins, ( uint64_t )m_Config.m_nMaxProviderPins * sizeof( SIRProviderPinSlot ), alignof( SIRProviderPinSlot ),
	         m_Config.m_nMetadataDomain );
	release( m_pStaging, ( uint64_t )m_Config.m_nMaxStaging * sizeof( SIRStagingSlot ), alignof( SIRStagingSlot ),
	         m_Config.m_nMetadataDomain );
	release( m_pSlots, ( uint64_t )m_Config.m_nMaxResources * sizeof( SIRGraphSlot ), alignof( SIRGraphSlot ), m_Config.m_nMetadataDomain );
}

void *SIRResourceGraph::allocateKeyIndex( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	SIRResourceGraph *pGraph = ( SIRResourceGraph * )pContext;
	if ( pGraph == nullptr || nAlignment > UINT32_MAX ) {
		return nullptr;
	}
	return pGraph->allocate( ( uint64_t )nBytes, ( uint32_t )nAlignment, pGraph->m_Config.m_nMetadataDomain );
}

void SIRResourceGraph::releaseKeyIndex( void *pContext, void *pData, size_t nBytes, size_t nAlignment ) noexcept
{
	SIRResourceGraph *pGraph = ( SIRResourceGraph * )pContext;
	if ( pGraph != nullptr && pData != nullptr ) {
		pGraph->release( pData, ( uint64_t )nBytes, ( uint32_t )nAlignment, pGraph->m_Config.m_nMetadataDomain );
	}
}

void *SIRResourceGraph::allocate( uint64_t nSize, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	if ( !nSize || nSize > SIZE_MAX || !isPowerOfTwo( nAlignment ) || !m_Config.m_Allocator.m_pfnAllocate ||
	     nSize > UINT64_MAX - m_nCommittedBytes ||
	     ( m_Config.m_nMaxCommittedBytes && nSize > m_Config.m_nMaxCommittedBytes - m_nCommittedBytes ) ) {
		return nullptr;
	}
	void *pResult = m_Config.m_Allocator.m_pfnAllocate( m_Config.m_Allocator.m_pContext, nSize, nAlignment, nDomain );
	if ( pResult ) {
		m_nCommittedBytes += nSize;
		if ( m_nCommittedBytes > m_nPeakCommittedBytes ) {
			m_nPeakCommittedBytes = m_nCommittedBytes;
		}
	}
	return pResult;
}

void SIRResourceGraph::release( void *pData, uint64_t nSize, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	if ( pData ) {
		m_Config.m_Allocator.m_pfnFree( m_Config.m_Allocator.m_pContext, pData, nSize, nAlignment, nDomain );
		m_nCommittedBytes -= nSize;
	}
}

bool SIRResourceGraph::isWriterThread( void ) const noexcept
{
	return m_bReady && std::this_thread::get_id() == m_OwnerThread;
}

bool SIRResourceGraph::isReady( void ) const noexcept
{
	return m_bReady;
}

SIRResourceGraphWriter *SIRResourceGraph::getWriter( void ) noexcept
{
	return isWriterThread() ? &m_Writer : nullptr;
}

SIRResourceGraphStats SIRResourceGraph::getStats( void ) const noexcept
{
	SIRResourceGraphStats result = {};
	result.m_nActiveReaders = m_nActiveReaders.load( std::memory_order_seq_cst );
	if ( !isWriterThread() ) {
		return result;
	}
	result.m_nCommittedBytes = m_nCommittedBytes;
	result.m_nPeakCommittedBytes = m_nPeakCommittedBytes;
	result.m_nPayloadBytes = m_nPayloadBytes;
	result.m_nUniqueChunks = m_nUniqueChunks;
	result.m_nRetiredRecords = m_nRetiredRecords;
	result.m_nLiveResources = m_KeyIndex.getSize();
	return result;
}

int32_t SIRResourceGraph::findSlot( const SIRResourceKey &rKey ) const noexcept
{
	const uint32_t *pSlot = m_KeyIndex.find( rKey );
	return pSlot != nullptr ? ( int32_t )*pSlot : -1;
}

int32_t SIRResourceGraph::findEmptySlot( void ) const noexcept
{
	for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxResources; ++iSlot ) {
		if ( !m_pSlots[iSlot].m_pPublished.load( std::memory_order_seq_cst ) ) {
			return ( int32_t )iSlot;
		}
	}
	return -1;
}

SIRPublishedResourceRecord *SIRResourceGraph::allocateRecord( uint32_t nChunkCount, uint32_t nDependencyCount ) noexcept
{
	const uint64_t nChunks = ( uint64_t )nChunkCount * sizeof( SIRRecordChunk );
	const uint64_t nDeps = ( uint64_t )nDependencyCount * sizeof( SIRResourceDependency );
	const uint64_t nBase = sizeof( SIRPublishedResourceRecord );
	if ( nChunks > UINT64_MAX - nBase || nDeps > UINT64_MAX - ( nBase + nChunks ) ) {
		return nullptr;
	}
	const uint64_t nTotal = nBase + nChunks + nDeps;
	uint8_t *pMemory = ( uint8_t * )allocate( nTotal, alignof( SIRPublishedResourceRecord ), m_Config.m_nMetadataDomain );
	if ( !pMemory ) {
		return nullptr;
	}
	memset( pMemory, 0, ( size_t )nTotal );
	SIRPublishedResourceRecord *pRecord = ( SIRPublishedResourceRecord * )pMemory;
	pRecord->m_nAllocationBytes = nTotal;
	pRecord->m_pChunks = ( SIRRecordChunk * )( pMemory + nBase );
	pRecord->m_pDependencies = ( SIRResourceDependency * )( pMemory + nBase + nChunks );
	pRecord->m_Info.m_nChunkCount = nChunkCount;
	pRecord->m_Info.m_nDependencyCount = nDependencyCount;
	return pRecord;
}

void SIRResourceGraph::destroyRecord( SIRPublishedResourceRecord *pRecord ) noexcept
{
	if ( !pRecord ) {
		return;
	}
	for ( uint32_t iChunk = 0; iChunk < pRecord->m_Info.m_nChunkCount; ++iChunk ) {
		SIRSharedResourceChunk *pShared = pRecord->m_pChunks[iChunk].m_pShared;
		if ( !pShared ) {
			continue;
		}
		--pShared->m_nReferences;
		if ( pShared->m_nReferences ) {
			continue;
		}
		SIRSharedResourceChunk **pLink = &m_pSharedChunks;
		while ( *pLink && *pLink != pShared ) {
			pLink = &( *pLink )->m_pNext;
		}
		if ( *pLink ) {
			*pLink = pShared->m_pNext;
		}
		m_nPayloadBytes -= pShared->m_nSize;
		--m_nUniqueChunks;
		release( pShared->m_pData, pShared->m_nSize, pShared->m_nAlignment, pShared->m_nDomain );
		release( pShared, sizeof( SIRSharedResourceChunk ), alignof( SIRSharedResourceChunk ), m_Config.m_nMetadataDomain );
	}
	release( pRecord, pRecord->m_nAllocationBytes, alignof( SIRPublishedResourceRecord ), m_Config.m_nMetadataDomain );
}

void SIRResourceGraph::pushRetired( SIRPublishedResourceRecord *pRecord ) noexcept
{
	if ( pRecord ) {
		pRecord->m_pRetiredNext = m_pRetired;
		m_pRetired = pRecord;
		++m_nRetiredRecords;
	}
}

void SIRResourceGraph::publishRecord( uint32_t iSlot, SIRPublishedResourceRecord *pRecord ) noexcept
{
	SIRPublishedResourceRecord *pPrior = m_pSlots[iSlot].m_pPublished.exchange( pRecord, std::memory_order_seq_cst );
	pushRetired( pPrior );
}

SIRResourceResult SIRResourceGraph::checkHandle( SIRResourceHandle hResource, SIRPublishedResourceRecord **pRecord ) const noexcept
{
	if ( !pRecord || hResource.m_nSlot >= m_Config.m_nMaxResources ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	*pRecord = m_pSlots[hResource.m_nSlot].m_pPublished.load( std::memory_order_seq_cst );
	if ( !*pRecord || !validHandle( hResource, ( *pRecord )->m_Handle ) ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraphWriter::request( const SIRResourceKey &rKey, SIRResourceHandle *pHandle ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !pHandle || !rKey.m_nTypeId ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const int32_t iExisting = m_pGraph->findSlot( rKey );
	if ( iExisting >= 0 ) {
		*pHandle = m_pGraph->m_pSlots[iExisting].m_pPublished.load( std::memory_order_seq_cst )->m_Handle;
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	const int32_t iSlot = m_pGraph->findEmptySlot();
	if ( iSlot < 0 ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRPublishedResourceRecord *pRecord = m_pGraph->allocateRecord( 0, 0 );
	if ( !pRecord ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	pRecord->m_Info.m_Key = rKey;
	pRecord->m_Info.m_eState = SIR_RESOURCE_STATE_REQUESTED;
	pRecord->m_Handle = { ( uint32_t )iSlot, m_pGraph->m_pSlots[iSlot].m_nNextGeneration, rKey.m_nTypeId, 0 };
	if ( m_pGraph->m_KeyIndex.tryInsert( rKey, ( uint32_t )iSlot ) != SIR_CONTAINER_OK ) {
		m_pGraph->destroyRecord( pRecord );
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	m_pGraph->m_pSlots[iSlot].m_nNextGeneration = nextGeneration( pRecord->m_Handle.m_nGeneration );
	m_pGraph->publishRecord( ( uint32_t )iSlot, pRecord );
	*pHandle = pRecord->m_Handle;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraphWriter::allocateStaging( uint64_t nSize, uint32_t nAlignment, uint32_t nDomainId,
                                                           SIRResourceStagingHandle *pHandle, SIRResourceMutableBytes *pBytes ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !pHandle || !pBytes || !nSize || !isPowerOfTwo( nAlignment ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	for ( uint32_t iSlot = 0; iSlot < m_pGraph->m_Config.m_nMaxStaging; ++iSlot ) {
		SIRStagingSlot *pSlot = &m_pGraph->m_pStaging[iSlot];
		if ( pSlot->m_pData ) {
			continue;
		}
		uint8_t *pMemory = ( uint8_t * )m_pGraph->allocate( nSize, nAlignment, nDomainId );
		if ( !pMemory ) {
			return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
		}
		pSlot->m_nGeneration = nextGeneration( pSlot->m_nGeneration );
		pSlot->m_pData = pMemory;
		pSlot->m_nSize = nSize;
		pSlot->m_nAlignment = nAlignment;
		pSlot->m_nDomain = nDomainId;
		*pHandle = { iSlot, pSlot->m_nGeneration };
		*pBytes = { pMemory, nSize };
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
}

void SIRResourceGraph::releaseStagingSlot( uint32_t iSlot ) noexcept
{
	SIRStagingSlot *pSlot = &m_pStaging[iSlot];
	release( pSlot->m_pData, pSlot->m_nSize, pSlot->m_nAlignment, pSlot->m_nDomain );
	pSlot->m_pData = nullptr;
	pSlot->m_nSize = 0;
	pSlot->m_nAlignment = 0;
}

SIRResourceResult SIRResourceGraphWriter::cancelStaging( SIRResourceStagingHandle hStaging ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( hStaging.m_nSlot >= m_pGraph->m_Config.m_nMaxStaging ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	const SIRStagingSlot &rSlot = m_pGraph->m_pStaging[hStaging.m_nSlot];
	if ( !rSlot.m_pData || rSlot.m_nGeneration != hStaging.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	m_pGraph->releaseStagingSlot( hStaging.m_nSlot );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRSharedResourceChunk *SIRResourceGraph::findMatchingChunk( const SIRResourceContentKey &rKey, const uint8_t *pData, uint64_t nSize,
                                                             uint32_t nAlignment,
                                                             const SIRPublishedResourceRecord *pPending ) const noexcept
{
	SIRSharedResourceChunk *pFound = nullptr;
	for ( uint32_t iPass = 0; iPass < 2; ++iPass ) {
		const SIRSharedResourceChunk *pCurrent = iPass == 0 ? m_pSharedChunks : nullptr;
		const uint32_t nPending = iPass == 1 && pPending ? pPending->m_Info.m_nChunkCount : 0;
		uint32_t iChunk = 0;
		while ( pCurrent || iChunk < nPending ) {
			SIRSharedResourceChunk *pCandidate =
			    iPass == 0 ? ( SIRSharedResourceChunk * )pCurrent : pPending->m_pChunks[iChunk++].m_pShared;
			if ( iPass == 0 ) {
				pCurrent = pCurrent->m_pNext;
			}
			if ( !pCandidate || !sameContentKey( rKey, pCandidate->m_ContentKey ) ) {
				continue;
			}
			if ( pData && ( pCandidate->m_nSize != nSize || pCandidate->m_nAlignment < nAlignment ||
			                memcmp( pCandidate->m_pData, pData, ( size_t )nSize ) != 0 ) ) {
				continue;
			}
			if ( pFound && pFound != pCandidate &&
			     ( pFound->m_nSize != pCandidate->m_nSize ||
			       memcmp( pFound->m_pData, pCandidate->m_pData, ( size_t )pFound->m_nSize ) != 0 ) ) {
				return nullptr; // Hash collision: never alias by key alone.
			}
			pFound = pCandidate;
		}
	}
	return pFound;
}

bool SIRResourceGraph::reachesKey( const SIRResourceKey &rCurrent, const SIRResourceKey &rTarget ) noexcept
{
	if ( sameKey( rCurrent, rTarget ) ) {
		return true;
	}
	const int32_t iRoot = findSlot( rCurrent );
	if ( iRoot < 0 ) {
		return false;
	}
	m_nTraversalStamp = nextGeneration( m_nTraversalStamp );
	if ( m_nTraversalStamp == 1 ) {
		for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxResources; ++iSlot ) {
			m_pSlots[iSlot].m_nVisitedStamp = 0;
		}
	}
	uint32_t nHead = 0;
	uint32_t nTail = 1;
	m_pTraversalQueue[0] = ( uint32_t )iRoot;
	m_pSlots[iRoot].m_nVisitedStamp = m_nTraversalStamp;
	while ( nHead < nTail ) {
		const uint32_t iSlot = m_pTraversalQueue[nHead++];
		const SIRPublishedResourceRecord *pRecord = m_pSlots[iSlot].m_pPublished.load( std::memory_order_seq_cst );
		for ( uint32_t iEdge = 0; iEdge < pRecord->m_Info.m_nDependencyCount; ++iEdge ) {
			const SIRResourceKey &rNext = pRecord->m_pDependencies[iEdge].m_Target;
			if ( sameKey( rNext, rTarget ) ) {
				return true;
			}
			const int32_t iNext = findSlot( rNext );
			if ( iNext < 0 || m_pSlots[iNext].m_nVisitedStamp == m_nTraversalStamp ) {
				continue;
			}
			m_pSlots[iNext].m_nVisitedStamp = m_nTraversalStamp;
			m_pTraversalQueue[nTail++] = ( uint32_t )iNext;
		}
	}
	return false;
}

SIRResourceResult SIRResourceGraph::validateDependencies( const SIRResourcePublication &rPublication ) noexcept
{
	for ( uint32_t iEdge = 0; iEdge < rPublication.m_nDependencyCount; ++iEdge ) {
		const SIRResourceDependency &rEdge = rPublication.m_pDependencies[iEdge];
		if ( !rEdge.m_Target.m_nTypeId || rEdge.m_eKind > SIR_RESOURCE_EDGE_KIND_STREAMED ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
		if ( rEdge.m_eKind == SIR_RESOURCE_EDGE_KIND_REQUIRED && findSlot( rEdge.m_Target ) < 0 ) {
			return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
		}
		if ( reachesKey( rEdge.m_Target, rPublication.m_Key ) ) {
			return SIR_RESOURCE_RESULT_DEPENDENCY_CYCLE;
		}
	}
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraph::prepareChunk( SIRPublishedResourceRecord *pRecord, uint32_t iChunk,
                                                  const SIRResourceChunkPublication &rInput ) noexcept
{
	SIRRecordChunk &rChunk = pRecord->m_pChunks[iChunk];
	rChunk.m_iStagingSlot = UINT32_MAX;
	for ( uint32_t iEarlier = 0; iEarlier < iChunk; ++iEarlier ) {
		if ( pRecord->m_pChunks[iEarlier].m_Info.m_Id.m_nValue == rInput.m_Id.m_nValue ) {
			return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
		}
	}
	SIRSharedResourceChunk *pShared = nullptr;
	if ( rInput.m_bReuseExisting ) {
		pShared = findMatchingChunk( rInput.m_ContentKey, nullptr, 0, 0, pRecord );
		if ( !pShared ) {
			return SIR_RESOURCE_RESULT_MISSING_CHUNK;
		}
	} else {
		if ( rInput.m_Staging.m_nSlot >= m_Config.m_nMaxStaging ) {
			return SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		SIRStagingSlot &rStaging = m_pStaging[rInput.m_Staging.m_nSlot];
		if ( !rStaging.m_pData || rStaging.m_nGeneration != rInput.m_Staging.m_nGeneration ) {
			return SIR_RESOURCE_RESULT_INVALID_HANDLE;
		}
		for ( uint32_t iEarlier = 0; iEarlier < iChunk; ++iEarlier ) {
			if ( pRecord->m_pChunks[iEarlier].m_iStagingSlot == rInput.m_Staging.m_nSlot ) {
				return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
			}
		}
		rChunk.m_iStagingSlot = rInput.m_Staging.m_nSlot;
		pShared = findMatchingChunk( rInput.m_ContentKey, rStaging.m_pData, rStaging.m_nSize, rStaging.m_nAlignment, pRecord );
		if ( !pShared ) {
			pShared = ( SIRSharedResourceChunk * )allocate( sizeof( SIRSharedResourceChunk ), alignof( SIRSharedResourceChunk ),
			                                                m_Config.m_nMetadataDomain );
			if ( !pShared ) {
				return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
			}
			memset( pShared, 0, sizeof( SIRSharedResourceChunk ) );
			pShared->m_ContentKey = rInput.m_ContentKey;
			pShared->m_pData = rStaging.m_pData;
			pShared->m_nSize = rStaging.m_nSize;
			pShared->m_nAlignment = rStaging.m_nAlignment;
			pShared->m_nDomain = rStaging.m_nDomain;
			rChunk.m_bNewShared = true;
		}
	}
	rChunk.m_pShared = pShared;
	rChunk.m_Info = { rInput.m_Id, pShared->m_nAlignment, pShared->m_nSize, rInput.m_ContentKey };
	if ( rChunk.m_Info.m_nBytes > UINT64_MAX - pRecord->m_Info.m_nLogicalBytes ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	pRecord->m_Info.m_nLogicalBytes += rChunk.m_Info.m_nBytes;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

void SIRResourceGraph::abandonUnpublishedRecord( SIRPublishedResourceRecord *pRecord ) noexcept
{
	for ( uint32_t iChunk = 0; iChunk < pRecord->m_Info.m_nChunkCount; ++iChunk ) {
		if ( pRecord->m_pChunks[iChunk].m_bNewShared ) {
			release( pRecord->m_pChunks[iChunk].m_pShared, sizeof( SIRSharedResourceChunk ), alignof( SIRSharedResourceChunk ),
			         m_Config.m_nMetadataDomain );
		}
	}
	release( pRecord, pRecord->m_nAllocationBytes, alignof( SIRPublishedResourceRecord ), m_Config.m_nMetadataDomain );
}

void SIRResourceGraph::adoptPublishedRecordChunks( SIRPublishedResourceRecord *pRecord ) noexcept
{
	for ( uint32_t iChunk = 0; iChunk < pRecord->m_Info.m_nChunkCount; ++iChunk ) {
		SIRRecordChunk &rChunk = pRecord->m_pChunks[iChunk];
		SIRSharedResourceChunk *pShared = rChunk.m_pShared;
		if ( rChunk.m_bNewShared ) {
			pShared->m_pNext = m_pSharedChunks;
			m_pSharedChunks = pShared;
			m_nPayloadBytes += pShared->m_nSize;
			++m_nUniqueChunks;
		}
		++pShared->m_nReferences;
		if ( rChunk.m_iStagingSlot != UINT32_MAX ) {
			if ( rChunk.m_bNewShared ) {
				m_pStaging[rChunk.m_iStagingSlot].m_pData = nullptr;
			}
			releaseStagingSlot( rChunk.m_iStagingSlot );
		}
	}
}

SIRResourceResult SIRResourceGraphWriter::publish( const SIRResourcePublication &rPublication, SIRResourceHandle *pHandle ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	if ( !pHandle || !rPublication.m_Key.m_nTypeId || !rPublication.m_nSchemaVersion ||
	     rPublication.m_eKind > SIR_RESOURCE_KIND_COMPOSITION || ( rPublication.m_nChunkCount && !rPublication.m_pChunks ) ||
	     ( rPublication.m_nDependencyCount && !rPublication.m_pDependencies ) ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const SIRResourceResult eDependencies = m_pGraph->validateDependencies( rPublication );
	if ( eDependencies != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eDependencies;
	}
	const int32_t iPrevious = m_pGraph->findSlot( rPublication.m_Key );
	const int32_t iSlot = iPrevious >= 0 ? iPrevious : m_pGraph->findEmptySlot();
	if ( iSlot < 0 ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	SIRPublishedResourceRecord *pPrior =
	    iPrevious >= 0 ? m_pGraph->m_pSlots[iSlot].m_pPublished.load( std::memory_order_seq_cst ) : nullptr;
	if ( pPrior && pPrior->m_Info.m_eState == SIR_RESOURCE_STATE_RESIDENT && pPrior->m_Info.m_eKind != rPublication.m_eKind ) {
		return SIR_RESOURCE_RESULT_TYPE_MISMATCH;
	}
	if ( pPrior && pPrior->m_Info.m_nRevision == UINT64_MAX ) {
		return SIR_RESOURCE_RESULT_INVALID_FORMAT;
	}
	SIRPublishedResourceRecord *pRecord = m_pGraph->allocateRecord( rPublication.m_nChunkCount, rPublication.m_nDependencyCount );
	if ( !pRecord ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	pRecord->m_Info.m_Key = rPublication.m_Key;
	pRecord->m_Info.m_eKind = rPublication.m_eKind;
	pRecord->m_Info.m_eState = SIR_RESOURCE_STATE_RESIDENT;
	pRecord->m_Info.m_nSchemaVersion = rPublication.m_nSchemaVersion;
	pRecord->m_Info.m_nFormatId = rPublication.m_nFormatId;
	pRecord->m_Info.m_nRevision = pPrior ? pPrior->m_Info.m_nRevision + 1 : 1;
	pRecord->m_Handle =
	    pPrior ? pPrior->m_Handle
	           : SIRResourceHandle{ ( uint32_t )iSlot, m_pGraph->m_pSlots[iSlot].m_nNextGeneration, rPublication.m_Key.m_nTypeId, 0 };
	for ( uint32_t iEdge = 0; iEdge < rPublication.m_nDependencyCount; ++iEdge ) {
		pRecord->m_pDependencies[iEdge] = rPublication.m_pDependencies[iEdge];
	}

	for ( uint32_t iChunk = 0; iChunk < rPublication.m_nChunkCount; ++iChunk ) {
		const SIRResourceResult eResult = m_pGraph->prepareChunk( pRecord, iChunk, rPublication.m_pChunks[iChunk] );
		if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
			m_pGraph->abandonUnpublishedRecord( pRecord );
			return eResult;
		}
	}
	if ( !pPrior && m_pGraph->m_KeyIndex.tryInsert( rPublication.m_Key, ( uint32_t )iSlot ) != SIR_CONTAINER_OK ) {
		m_pGraph->abandonUnpublishedRecord( pRecord );
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	m_pGraph->adoptPublishedRecordChunks( pRecord );
	if ( !pPrior ) {
		m_pGraph->m_pSlots[iSlot].m_nNextGeneration = nextGeneration( pRecord->m_Handle.m_nGeneration );
	}
	m_pGraph->publishRecord( ( uint32_t )iSlot, pRecord );
	*pHandle = pRecord->m_Handle;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraph::buildReplacement( SIRPublishedResourceRecord *pCurrent, uint32_t iExcludeChunk, SIRResourceState eState,
                                                      SIRPublishedResourceRecord **pReplacement ) noexcept
{
	if ( !pCurrent || !pReplacement ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	const uint32_t nOldChunks = pCurrent->m_Info.m_nChunkCount;
	const uint32_t nChunks = iExcludeChunk < nOldChunks ? nOldChunks - 1 : nOldChunks;
	SIRPublishedResourceRecord *pResult = allocateRecord( nChunks, pCurrent->m_Info.m_nDependencyCount );
	if ( !pResult ) {
		return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
	}
	pResult->m_Info = pCurrent->m_Info;
	pResult->m_Info.m_nRevision++;
	pResult->m_Info.m_nChunkCount = nChunks;
	pResult->m_Info.m_nLogicalBytes = 0;
	pResult->m_Info.m_eState = eState;
	pResult->m_Handle = pCurrent->m_Handle;
	for ( uint32_t iEdge = 0; iEdge < pResult->m_Info.m_nDependencyCount; ++iEdge ) {
		pResult->m_pDependencies[iEdge] = pCurrent->m_pDependencies[iEdge];
	}
	uint32_t iNext = 0;
	for ( uint32_t iChunk = 0; iChunk < nOldChunks; ++iChunk ) {
		if ( iChunk == iExcludeChunk ) {
			continue;
		}
		pResult->m_pChunks[iNext] = pCurrent->m_pChunks[iChunk];
		pResult->m_pChunks[iNext].m_bNewShared = false;
		pResult->m_pChunks[iNext].m_iStagingSlot = UINT32_MAX;
		++pResult->m_pChunks[iNext].m_pShared->m_nReferences;
		pResult->m_Info.m_nLogicalBytes += pResult->m_pChunks[iNext].m_Info.m_nBytes;
		++iNext;
	}
	*pReplacement = pResult;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraphWriter::evictChunk( SIRResourceHandle hResource, SIRResourceChunkId id ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	SIRPublishedResourceRecord *pCurrent = nullptr;
	SIRResourceResult eResult = m_pGraph->checkHandle( hResource, &pCurrent );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t iChunk = 0; iChunk < pCurrent->m_Info.m_nChunkCount; ++iChunk ) {
		if ( pCurrent->m_pChunks[iChunk].m_Info.m_Id.m_nValue != id.m_nValue ) {
			continue;
		}
		SIRPublishedResourceRecord *pNew = nullptr;
		eResult = m_pGraph->buildReplacement(
		    pCurrent, iChunk, pCurrent->m_Info.m_nChunkCount == 1 ? SIR_RESOURCE_STATE_REQUESTED : SIR_RESOURCE_STATE_RESIDENT, &pNew );
		if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
			m_pGraph->publishRecord( hResource.m_nSlot, pNew );
		}
		return eResult;
	}
	return SIR_RESOURCE_RESULT_MISSING_CHUNK;
}

SIRResourceResult SIRResourceGraphWriter::markFailed( SIRResourceHandle hResource, SIRResourceResult eCause ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	SIRPublishedResourceRecord *pCurrent = nullptr;
	SIRResourceResult eResult = m_pGraph->checkHandle( hResource, &pCurrent );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( eCause == SIR_RESOURCE_RESULT_SUCCESS || pCurrent->m_Info.m_eState == SIR_RESOURCE_STATE_RESIDENT ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT; // Keep the previously published good revision.
	}
	SIRPublishedResourceRecord *pNew = nullptr;
	eResult = m_pGraph->buildReplacement( pCurrent, UINT32_MAX, SIR_RESOURCE_STATE_FAILED, &pNew );
	if ( eResult == SIR_RESOURCE_RESULT_SUCCESS ) {
		m_pGraph->publishRecord( hResource.m_nSlot, pNew );
	}
	return eResult;
}

SIRResourceResult SIRResourceGraphWriter::retire( SIRResourceHandle hResource ) noexcept
{
	if ( !m_pGraph->isWriterThread() ) {
		return SIR_RESOURCE_RESULT_WRITER_VIOLATION;
	}
	SIRPublishedResourceRecord *pRecord = nullptr;
	SIRResourceResult eResult = m_pGraph->checkHandle( hResource, &pRecord );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( !m_pGraph->m_KeyIndex.erase( pRecord->m_Info.m_Key ) ) {
		return SIR_RESOURCE_RESULT_FAILED;
	}
	m_pGraph->publishRecord( hResource.m_nSlot, nullptr );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

uint32_t SIRResourceGraph::collectRetiredInternal( uint32_t nMaxRecords ) noexcept
{
	if ( !nMaxRecords ) {
		return 0;
	}
	std::lock_guard<std::mutex> pinLock( m_PinMutex );
	if ( m_nActiveReaders.load( std::memory_order_seq_cst ) != 0 ) {
		return 0;
	}
	uint32_t nCollected = 0;
	uint32_t nVisited = 0;
	SIRPublishedResourceRecord **pLink = &m_pRetired;
	while ( *pLink && nVisited < nMaxRecords ) {
		++nVisited;
		SIRPublishedResourceRecord *pCurrent = *pLink;
		if ( pCurrent->m_nProviderPins ) {
			pLink = &pCurrent->m_pRetiredNext;
			continue;
		}
		*pLink = pCurrent->m_pRetiredNext;
		destroyRecord( pCurrent );
		--m_nRetiredRecords;
		++nCollected;
	}
	return nCollected;
}

uint32_t SIRResourceGraphWriter::collectRetired( uint32_t nMaxRecords ) noexcept
{
	return m_pGraph->isWriterThread() ? m_pGraph->collectRetiredInternal( nMaxRecords ) : 0;
}

SIRResourceReadScope::SIRResourceReadScope( const SIRResourceGraph &rGraph ) noexcept
    : m_pGraph( rGraph.isReady() ? &rGraph : nullptr ), m_nReaderToken( 0 )
{
	if ( m_pGraph ) {
		m_pGraph->m_nActiveReaders.fetch_add( 1, std::memory_order_seq_cst );
		m_nReaderToken = 1;
	}
}

SIRResourceReadScope::~SIRResourceReadScope( void ) noexcept
{
	if ( m_nReaderToken ) {
		m_pGraph->m_nActiveReaders.fetch_sub( 1, std::memory_order_seq_cst );
	}
}

bool SIRResourceReadScope::isActive( void ) const noexcept
{
	return m_nReaderToken != 0;
}

SIRResourceResult SIRResourceGraph::find( const SIRResourceReadScope &rScope, const SIRResourceKey &rKey,
                                          SIRResourceHandle *pHandle ) const noexcept
{
	if ( !rScope.isActive() || rScope.m_pGraph != this || !pHandle || !rKey.m_nTypeId ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	for ( uint32_t iSlot = 0; iSlot < m_Config.m_nMaxResources; ++iSlot ) {
		const SIRPublishedResourceRecord *pRecord = m_pSlots[iSlot].m_pPublished.load( std::memory_order_seq_cst );
		if ( pRecord && sameKey( pRecord->m_Info.m_Key, rKey ) ) {
			*pHandle = pRecord->m_Handle;
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
	}
	return SIR_RESOURCE_RESULT_INVALID_HANDLE;
}

SIRResourceResult SIRResourceGraph::resolve( const SIRResourceReadScope &rScope, SIRResourceHandle hResource,
                                             SIRResourceView *pView ) const noexcept
{
	if ( !rScope.isActive() || rScope.m_pGraph != this || !pView || hResource.m_nSlot >= m_Config.m_nMaxResources ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRPublishedResourceRecord *pRecord = m_pSlots[hResource.m_nSlot].m_pPublished.load( std::memory_order_seq_cst );
	if ( !pRecord || !validHandle( hResource, pRecord->m_Handle ) ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	if ( pRecord->m_Info.m_eState != SIR_RESOURCE_STATE_RESIDENT ) {
		return SIR_RESOURCE_RESULT_NOT_RESIDENT;
	}
	*pView = SIRResourceView( &rScope, pRecord );
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceGraph::resolveDependency( const SIRResourceReadScope &rScope, const SIRResourceView &rParent,
                                                       uint32_t iDependency, SIRResourceView *pChild ) const noexcept
{
	if ( !pChild || rParent.m_pScope != &rScope || !rParent.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	SIRResourceDependency edge = {};
	SIRResourceResult eResult = rParent.getDependency( iDependency, &edge );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	SIRResourceHandle hChild = {};
	eResult = find( rScope, edge.m_Target, &hChild );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
	}
	return resolve( rScope, hChild, pChild );
}

SIRResourceResult SIRResourceGraph::pinForProvider( const SIRResourceReadScope &rScope, const SIRResourceView &rView, uint32_t nProviderId,
                                                    SIRResourcePinHandle *pPin ) const noexcept
{
	if ( !pPin || !nProviderId || !rScope.isActive() || rScope.m_pGraph != this || rView.m_pScope != &rScope || !rView.isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	std::lock_guard<std::mutex> pinLock( m_PinMutex );
	for ( uint32_t iPin = 0; iPin < m_Config.m_nMaxProviderPins; ++iPin ) {
		SIRProviderPinSlot &rPin = m_pProviderPins[iPin];
		if ( rPin.m_pRecord ) {
			continue;
		}
		rPin.m_nGeneration = nextGeneration( rPin.m_nGeneration );
		rPin.m_pRecord = ( SIRPublishedResourceRecord * )rView.m_pRecord;
		rPin.m_nProviderId = nProviderId;
		++rPin.m_pRecord->m_nProviderPins;
		*pPin = { iPin, rPin.m_nGeneration };
		return SIR_RESOURCE_RESULT_SUCCESS;
	}
	return SIR_RESOURCE_RESULT_OUT_OF_BUDGET;
}

SIRResourceResult SIRResourceGraph::releaseProviderPin( SIRResourcePinHandle hPin ) const noexcept
{
	if ( hPin.m_nSlot >= m_Config.m_nMaxProviderPins || !m_bReady ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	std::lock_guard<std::mutex> pinLock( m_PinMutex );
	SIRProviderPinSlot &rPin = m_pProviderPins[hPin.m_nSlot];
	if ( !rPin.m_pRecord || rPin.m_nGeneration != hPin.m_nGeneration ) {
		return SIR_RESOURCE_RESULT_INVALID_HANDLE;
	}
	--rPin.m_pRecord->m_nProviderPins;
	rPin.m_pRecord = nullptr;
	rPin.m_nProviderId = 0;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceView::SIRResourceView( void ) noexcept : m_pScope( nullptr ), m_pRecord( nullptr )
{
}

SIRResourceView::SIRResourceView( const SIRResourceReadScope *pScope, const SIRPublishedResourceRecord *pRecord ) noexcept
    : m_pScope( pScope ), m_pRecord( pRecord )
{
}

bool SIRResourceView::isValid( void ) const noexcept
{
	return m_pScope && m_pScope->isActive() && m_pRecord;
}

SIRResourceKind SIRResourceView::getKind( void ) const noexcept
{
	return isValid() ? m_pRecord->m_Info.m_eKind : SIR_RESOURCE_KIND_BLOB;
}

SIRResourceHandle SIRResourceView::getHandle( void ) const noexcept
{
	return isValid() ? m_pRecord->m_Handle : SIRResourceHandle{};
}

uint64_t SIRResourceView::getRevision( void ) const noexcept
{
	return isValid() ? m_pRecord->m_Info.m_nRevision : 0;
}

SIRResourceResult SIRResourceView::getInfo( SIRResourceInfo *pInfo ) const noexcept
{
	if ( !pInfo || !isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pInfo = m_pRecord->m_Info;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceView::getChunk( SIRResourceChunkId id, SIRResourceConstBytes *pBytes ) const noexcept
{
	if ( !pBytes || !isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	for ( uint32_t iChunk = 0; iChunk < m_pRecord->m_Info.m_nChunkCount; ++iChunk ) {
		const SIRRecordChunk &rChunk = m_pRecord->m_pChunks[iChunk];
		if ( rChunk.m_Info.m_Id.m_nValue == id.m_nValue ) {
			*pBytes = { rChunk.m_pShared->m_pData, rChunk.m_pShared->m_nSize };
			return SIR_RESOURCE_RESULT_SUCCESS;
		}
	}
	return SIR_RESOURCE_RESULT_MISSING_CHUNK;
}

SIRResourceResult SIRResourceView::getChunkInfo( uint32_t iChunk, SIRResourceChunkInfo *pInfo ) const noexcept
{
	if ( !pInfo || !isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( iChunk >= m_pRecord->m_Info.m_nChunkCount ) {
		return SIR_RESOURCE_RESULT_MISSING_CHUNK;
	}
	*pInfo = m_pRecord->m_pChunks[iChunk].m_Info;
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRResourceView::getDependency( uint32_t iEdge, SIRResourceDependency *pEdge ) const noexcept
{
	if ( !pEdge || !isValid() ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	if ( iEdge >= m_pRecord->m_Info.m_nDependencyCount ) {
		return SIR_RESOURCE_RESULT_DEPENDENCY_MISSING;
	}
	*pEdge = m_pRecord->m_pDependencies[iEdge];
	return SIR_RESOURCE_RESULT_SUCCESS;
}

SIRResourceResult SIRBlobResourceView::getBytes( SIRResourceChunkId id, SIRResourceByteRange range,
                                                 SIRResourceConstBytes *pBytes ) const noexcept
{
	SIRResourceConstBytes bytes = {};
	SIRResourceResult eResult = getChunk( id, &bytes );
	if ( eResult != SIR_RESOURCE_RESULT_SUCCESS ) {
		return eResult;
	}
	if ( !pBytes || range.m_nOffset > bytes.m_nSize || range.m_nSize > bytes.m_nSize - range.m_nOffset ) {
		return SIR_RESOURCE_RESULT_INVALID_ARGUMENT;
	}
	*pBytes = { bytes.m_pData + range.m_nOffset, range.m_nSize };
	return SIR_RESOURCE_RESULT_SUCCESS;
}

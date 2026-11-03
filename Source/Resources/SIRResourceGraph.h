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

#ifndef __SIRENGINE_SIRRESOURCEGRAPH_H__
#define __SIRENGINE_SIRRESOURCEGRAPH_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRResourceViews.h"
#include <Core/Containers/SIRFlatHashMap.h>

#include <atomic>
#include <mutex>
#include <thread>

class SIRResourceGraph;
struct SIRPublishedResourceRecord;
struct SIRSharedResourceChunk;
struct SIRGraphSlot;
struct SIRStagingSlot;
struct SIRProviderPinSlot;

// The key index belongs exclusively to the GRDG writer. Concurrent readers
// still resolve through published slots under their read scopes.
class SIRResourceKeyHash
{
public:
	size_t operator()( const SIRResourceKey &rKey ) const noexcept
	{
		uint64_t nHash = rKey.m_nAssetId;
		nHash ^= ( uint64_t )rKey.m_nTypeId * UINT64_C( 0x9e3779b97f4a7c15 );
		nHash ^= ( uint64_t )rKey.m_nVariantId * UINT64_C( 0xc2b2ae3d27d4eb4f );
		return ( size_t )nHash;
	}
};

class SIRResourceKeyEqual
{
public:
	bool operator()( const SIRResourceKey &rLeft, const SIRResourceKey &rRight ) const noexcept
	{
		return rLeft.m_nAssetId == rRight.m_nAssetId && rLeft.m_nTypeId == rRight.m_nTypeId && rLeft.m_nVariantId == rRight.m_nVariantId;
	}
};

// All GRDG allocations use this supplied domain-aware allocator. There is no
// implicit malloc/new fallback. NMDMemory supplies these callbacks when ported.
typedef struct sirResourceAllocator_s
{
	void *m_pContext;
	void *( *m_pfnAllocate )( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept;
	void ( *m_pfnFree )( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept;
} sirResourceAllocator_t;
typedef sirResourceAllocator_t SIRResourceAllocator;

typedef struct sirResourceGraphConfig_s
{
	SIRResourceAllocator m_Allocator;
	uint32_t m_nMetadataDomain;
	uint32_t m_nMaxResources;
	uint32_t m_nMaxStaging;
	uint32_t m_nMaxProviderPins;
	uint64_t m_nMaxCommittedBytes;
} sirResourceGraphConfig_t;
typedef sirResourceGraphConfig_t SIRResourceGraphConfig;

typedef struct sirResourceGraphStats_s
{
	uint64_t m_nCommittedBytes;
	uint64_t m_nPeakCommittedBytes;
	uint64_t m_nPayloadBytes;
	uint32_t m_nLiveResources;
	uint32_t m_nUniqueChunks;
	uint32_t m_nRetiredRecords;
	uint32_t m_nActiveReaders;
} sirResourceGraphStats_t;
typedef sirResourceGraphStats_t SIRResourceGraphStats;

class SIRResourceGraphWriter
{
private:
	SIRResourceGraph *m_pGraph;
	explicit SIRResourceGraphWriter( SIRResourceGraph &rGraph ) noexcept;

public:
	SIRResourceGraphWriter( const SIRResourceGraphWriter & ) = delete;
	SIRResourceGraphWriter &operator=( const SIRResourceGraphWriter & ) = delete;

	SIRResourceResult allocateStaging( uint64_t nSize, uint32_t nAlignment, uint32_t nDomainId, SIRResourceStagingHandle *pHandle,
	                                   SIRResourceMutableBytes *pBytes ) noexcept;
	SIRResourceResult cancelStaging( SIRResourceStagingHandle hStaging ) noexcept;
	SIRResourceResult request( const SIRResourceKey &rKey, SIRResourceHandle *pHandle ) noexcept;
	SIRResourceResult publish( const SIRResourcePublication &rPublication, SIRResourceHandle *pHandle ) noexcept;
	SIRResourceResult markFailed( SIRResourceHandle hResource, SIRResourceResult eCause ) noexcept;
	SIRResourceResult evictChunk( SIRResourceHandle hResource, SIRResourceChunkId id ) noexcept;
	SIRResourceResult retire( SIRResourceHandle hResource ) noexcept;
	uint32_t collectRetired( uint32_t nMaxRecords ) noexcept;

	friend class SIRResourceGraph;
};

// Fixed-capacity slot table, immutable published records, SC read entry and
// pointer publication. Retired records are freed only after reader quiescence
// and provider-pin completion. This is a conservative first GRDG slice.
class SIRResourceGraph
{
private:
	SIRResourceGraphConfig m_Config;
	SIRResourceGraphWriter m_Writer;
	std::thread::id m_OwnerThread;
	mutable std::atomic<uint32_t> m_nActiveReaders;
	mutable std::mutex m_PinMutex;
	sirContainerAllocator_t m_KeyIndexAllocator;
	SIRFlatHashMap<SIRResourceKey, uint32_t, SIRResourceKeyHash, SIRResourceKeyEqual> m_KeyIndex;
	SIRGraphSlot *m_pSlots;
	SIRStagingSlot *m_pStaging;
	SIRProviderPinSlot *m_pProviderPins;
	uint32_t *m_pTraversalQueue;
	uint32_t m_nTraversalStamp;
	SIRSharedResourceChunk *m_pSharedChunks;
	SIRPublishedResourceRecord *m_pRetired;
	uint64_t m_nCommittedBytes;
	uint64_t m_nPeakCommittedBytes;
	uint64_t m_nPayloadBytes;
	uint32_t m_nUniqueChunks;
	uint32_t m_nRetiredRecords;
	bool m_bReady;

	static void *allocateKeyIndex( void *pContext, size_t nBytes, size_t nAlignment ) noexcept;
	static void releaseKeyIndex( void *pContext, void *pData, size_t nBytes, size_t nAlignment ) noexcept;
	void *allocate( uint64_t nSize, uint32_t nAlignment, uint32_t nDomain ) noexcept;
	void release( void *pData, uint64_t nSize, uint32_t nAlignment, uint32_t nDomain ) noexcept;
	bool isWriterThread( void ) const noexcept;
	int32_t findSlot( const SIRResourceKey &rKey ) const noexcept;
	int32_t findEmptySlot( void ) const noexcept;
	SIRPublishedResourceRecord *allocateRecord( uint32_t nChunkCount, uint32_t nDependencyCount ) noexcept;
	void destroyRecord( SIRPublishedResourceRecord *pRecord ) noexcept;
	void pushRetired( SIRPublishedResourceRecord *pRecord ) noexcept;
	void publishRecord( uint32_t iSlot, SIRPublishedResourceRecord *pRecord ) noexcept;
	SIRSharedResourceChunk *findMatchingChunk( const SIRResourceContentKey &rKey, const uint8_t *pData, uint64_t nSize, uint32_t nAlignment,
	                                           const SIRPublishedResourceRecord *pPending ) const noexcept;
	bool reachesKey( const SIRResourceKey &rCurrent, const SIRResourceKey &rTarget ) noexcept;
	SIRResourceResult validateDependencies( const SIRResourcePublication &rPublication ) noexcept;
	SIRResourceResult checkHandle( SIRResourceHandle hResource, SIRPublishedResourceRecord **pRecord ) const noexcept;
	SIRResourceResult prepareChunk( SIRPublishedResourceRecord *pRecord, uint32_t iChunk,
	                                const SIRResourceChunkPublication &rInput ) noexcept;
	void abandonUnpublishedRecord( SIRPublishedResourceRecord *pRecord ) noexcept;
	void adoptPublishedRecordChunks( SIRPublishedResourceRecord *pRecord ) noexcept;
	SIRResourceResult buildReplacement( SIRPublishedResourceRecord *pCurrent, uint32_t iExcludeChunk, SIRResourceState eState,
	                                    SIRPublishedResourceRecord **pReplacement ) noexcept;
	void releaseStagingSlot( uint32_t iSlot ) noexcept;
	uint32_t collectRetiredInternal( uint32_t nMaxRecords ) noexcept;

public:
	explicit SIRResourceGraph( const SIRResourceGraphConfig &rConfig ) noexcept;
	~SIRResourceGraph( void ) noexcept;

	SIRResourceGraph( const SIRResourceGraph & ) = delete;
	SIRResourceGraph &operator=( const SIRResourceGraph & ) = delete;

	bool isReady( void ) const noexcept;
	SIRResourceGraphWriter *getWriter( void ) noexcept;
	SIRResourceGraphStats getStats( void ) const noexcept;
	SIRResourceResult find( const SIRResourceReadScope &rScope, const SIRResourceKey &rKey, SIRResourceHandle *pHandle ) const noexcept;
	SIRResourceResult resolve( const SIRResourceReadScope &rScope, SIRResourceHandle hResource, SIRResourceView *pView ) const noexcept;
	SIRResourceResult resolveDependency( const SIRResourceReadScope &rScope, const SIRResourceView &rParent, uint32_t iDependency,
	                                     SIRResourceView *pChild ) const noexcept;
	SIRResourceResult pinForProvider( const SIRResourceReadScope &rScope, const SIRResourceView &rView, uint32_t nProviderId,
	                                  SIRResourcePinHandle *pPin ) const noexcept;
	SIRResourceResult releaseProviderPin( SIRResourcePinHandle hPin ) const noexcept;

	friend class SIRResourceGraphWriter;
	friend class SIRResourceReadScope;
};

// A derived representation is owned by its backend, while GRDG tracks one
// distinct physical allocation and holds its source revision until release.
typedef struct sirDerivedResourceHandle_s
{
	uint32_t m_nSlot;
	uint32_t m_nGeneration;
} sirDerivedResourceHandle_t;
typedef sirDerivedResourceHandle_t SIRDerivedResourceHandle;

typedef struct sirDerivedResourceRegistration_s
{
	SIRDerivedResourceKey m_Key;
	SIRDerivedResourceAccounting m_Accounting;
	uint64_t m_nNativeToken;
} sirDerivedResourceRegistration_t;
typedef sirDerivedResourceRegistration_t SIRDerivedResourceRegistration;

// Release is invoked after the provider's completed fence has reached the
// representation's retirement fence. Return true only once native storage
// has actually been released. Callbacks must not re-enter the registry.
typedef struct sirDerivedProviderCallbacks_s
{
	void *pContext;
	bool ( *pfnReleaseNative )( void *pContext, uint64_t nNativeToken ) noexcept;
} sirDerivedProviderCallbacks_t;
typedef sirDerivedProviderCallbacks_t SIRDerivedProviderCallbacks;

typedef struct sirResourceDerivedConfig_s
{
	SIRResourceAllocator allocator;
	uint32_t nMetadataDomain;
	uint32_t nMaxRepresentations;
	uint32_t nMaxProviders;
	uint64_t nMaxTrackedBytes;
} sirResourceDerivedConfig_t;
typedef sirResourceDerivedConfig_t SIRResourceDerivedConfig;

// Byte totals represent independently allocated physical provider backing,
// including pending retirements. Canonical GRDG bytes remain in graph stats.
typedef struct sirResourceDerivedStats_s
{
	uint64_t nCommittedBytes[3];
	uint64_t nResidentBytes[3];
	uint64_t nPeakCommittedBytes[3];
	uint32_t nLiveRepresentations;
	uint32_t nPendingRetirements;
	uint32_t nRegisteredProviders;
} sirResourceDerivedStats_t;
typedef sirResourceDerivedStats_t SIRResourceDerivedStats;

typedef struct sirDerivedProviderSlot_s sirDerivedProviderSlot_t;
typedef struct sirDerivedRepresentationSlot_s sirDerivedRepresentationSlot_t;

// One writer thread, bounded metadata and generation-checked handles.
// Graph and allocator callback context must outlive the registry. Each
// registration pins its exact immutable source revision for the duration
// of backend ownership, even if a newer revision is later published.
class SIRResourceDerivedRegistry
{
private:
	SIRResourceGraph *m_pGraph;
	SIRResourceDerivedConfig m_Config;
	std::thread::id m_OwnerThread;
	sirDerivedProviderSlot_t *m_pProviders;
	sirDerivedRepresentationSlot_t *m_pRepresentations;
	SIRResourceDerivedStats m_Stats;
	uint64_t m_nTrackedBytes;
	bool m_bReady;

	bool isWriterThread( void ) const noexcept;
	int32_t findProvider( uint32_t nProviderId ) const noexcept;
	SIRResourceResult collectProvider( uint32_t iProvider ) noexcept;
	void releaseMetadata( void ) noexcept;

public:
	SIRResourceDerivedRegistry( SIRResourceGraph &rGraph, const SIRResourceDerivedConfig &rConfig ) noexcept;
	~SIRResourceDerivedRegistry( void ) noexcept;

	SIRResourceDerivedRegistry( const SIRResourceDerivedRegistry & ) = delete;
	SIRResourceDerivedRegistry &operator=( const SIRResourceDerivedRegistry & ) = delete;

	bool isReady( void ) const noexcept;
	SIRResourceResult registerProvider( uint32_t nProviderId, const SIRDerivedProviderCallbacks &rCallbacks ) noexcept;
	SIRResourceResult registerRepresentation( const SIRResourceReadScope &rScope, const SIRResourceView &rSource,
	                                          const SIRDerivedResourceRegistration &rRegistration,
	                                          SIRDerivedResourceHandle *pHandle ) noexcept;
	SIRResourceResult retireRepresentation( SIRDerivedResourceHandle hRepresentation, uint64_t nCompletionFence ) noexcept;
	SIRResourceResult updateCompletedFence( uint32_t nProviderId, uint64_t nCompletedFence ) noexcept;
	SIRResourceResult getAccounting( SIRDerivedResourceHandle hRepresentation, SIRDerivedResourceAccounting *pAccounting ) const noexcept;
	SIRResourceResult getStats( SIRResourceDerivedStats *pStats ) const noexcept;
};

#endif // __SIRENGINE_SIRRESOURCEGRAPH_H__

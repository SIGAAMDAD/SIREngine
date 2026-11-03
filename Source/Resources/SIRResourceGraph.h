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

#include <atomic>
#include <mutex>
#include <thread>

class SIRResourceGraph;
struct SIRPublishedResourceRecord;
struct SIRSharedResourceChunk;
struct SIRGraphSlot;
struct SIRStagingSlot;
struct SIRProviderPinSlot;

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

// Reserved for the next integration slice. These handles account for native
// provider memory; such memory is not a second owner of canonical CPU bytes.
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

class SIRResourceDerivedRegistry
{
public:
	SIRResourceResult registerRepresentation( const SIRDerivedResourceRegistration &rRegistration,
	                                          SIRDerivedResourceHandle *pHandle ) noexcept;
	SIRResourceResult retireRepresentation( SIRDerivedResourceHandle hRepresentation, uint64_t nCompletionFence ) noexcept;
	SIRResourceResult updateCompletedFence( uint32_t nProviderId, uint64_t nCompletedFence ) noexcept;
	SIRResourceResult getAccounting( SIRDerivedResourceHandle hRepresentation, SIRDerivedResourceAccounting *pAccounting ) const noexcept;
};

#endif // __SIRENGINE_SIRRESOURCEGRAPH_H__

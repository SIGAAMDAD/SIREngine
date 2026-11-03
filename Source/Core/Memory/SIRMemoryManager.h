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

#ifndef __SIRENGINE_SIRMEMORYMANAGER_H__
#define __SIRENGINE_SIRMEMORYMANAGER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRSync.h>

class SIRMemorySegment;

typedef enum sirMemoryDomain_e : uint32_t
{
	SIR_MEMORY_DOMAIN_CORE = 0,
	SIR_MEMORY_DOMAIN_GODOT = 1,
	SIR_MEMORY_DOMAIN_GAMEPLAY = 2,
	SIR_MEMORY_DOMAIN_EDITOR = 3,
	SIR_MEMORY_DOMAIN_NETWORKING = 4,
	SIR_MEMORY_DOMAIN_RESOURCE_RESIDENT = 5,
	SIR_MEMORY_DOMAIN_STREAMING = 6,
	SIR_MEMORY_DOMAIN_RENDERING_CPU = 7,
	SIR_MEMORY_DOMAIN_PHYSICS = 8,
	SIR_MEMORY_DOMAIN_AUDIO = 9,
	SIR_MEMORY_DOMAIN_JOB_SCRATCH = 10,
	SIR_MEMORY_DOMAIN_FRAME = 11,
	SIR_MEMORY_DOMAIN_THIRD_PARTY = 12,
	SIR_MEMORY_DOMAIN_COUNT = 13
} sirMemoryDomain_t;

typedef struct sirMemoryDomainStats_s
{
	uint64_t nCommittedBytes;
	uint64_t nPeakCommittedBytes;
	uint64_t nCommitLimitBytes;
} sirMemoryDomainStats_t;

typedef struct sirMemoryConfig_s
{
	uint64_t nReserveBytes;
	uint64_t nCommitBudgetBytes;
	uint64_t nMetadataBytes;
	uint32_t nMaxSegments;
} sirMemoryConfig_t;

typedef struct sirMemoryBudgetProfile_s
{
	uint64_t nInitialCommitBudgetBytes;
	uint64_t nMaximumCommitBudgetBytes;
} sirMemoryBudgetProfile_t;

typedef enum sirMemoryTraceType_e : uint32_t
{
	SIR_MEMORY_TRACE_COMMIT = 1,
	SIR_MEMORY_TRACE_DECOMMIT = 2,
	SIR_MEMORY_TRACE_METADATA = 3,
	SIR_MEMORY_TRACE_DENIED = 4
} sirMemoryTraceType_t;

typedef struct sirMemoryTraceEvent_s
{
	uint64_t nSequence;
	uint64_t nBytes;
	uint32_t nType;
	uint32_t nSegment;
} sirMemoryTraceEvent_t;

typedef struct sirMemoryStats_s
{
	uint64_t nReservedBytes;
	uint64_t nCommittedBytes;
	uint64_t nPeakCommittedBytes;
	uint64_t nCommitBudgetBytes;
	uint64_t nMetadataCommittedBytes;
	uint32_t nSegmentCount;
	uint64_t nMaximumCommitBudgetBytes;
	uint64_t nCommitDeniedCount;
	uint64_t nCommitCallCount;
	uint64_t nDecommitCallCount;
	uint64_t nDecommittedBytes;
	uint64_t nMetadataUsedBytes;
	uint64_t nTraceDroppedCount;
} sirMemoryStats_t;

// One OS reservation, one commit authority, stable segment descriptors.
// Initialize once. All segment objects and metadata live inside the reservation.
// Segment layout never changes; segment creation and page-state changes are cold.
class SIRMemoryManager
{
public:
	SIRMemoryManager( void );
	explicit SIRMemoryManager( const sirMemoryConfig_t &config );
	SIRMemoryManager( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget );
	~SIRMemoryManager();

	SIRMemoryManager( const SIRMemoryManager & ) = delete;
	SIRMemoryManager &operator=( const SIRMemoryManager & ) = delete;

	bool init( const sirMemoryConfig_t &config );
	bool initBudgeted( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget );
	bool setCommitBudgetBytes( uint64_t nNewBudget );
	bool isInitialized( void ) const;
	void shutdown( void );

	SIRMemorySegment *createSegment( const char *pszName, uint64_t nBytes, uint64_t nCommitLimitBytes = 0,
	                                 sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_GAMEPLAY );
	bool setDomainCommitLimit( sirMemoryDomain_t eDomain, uint64_t nLimitBytes );
	sirMemoryDomainStats_t getDomainStats( sirMemoryDomain_t eDomain ) const;

	// Initialization-time allocator metadata, backed by the SAME reservation.
	// Bump-only storage; individual allocations are not freed until shutdown().
	void *allocateMetadata( uint64_t nBytes, uint64_t nAlignment, sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_GAMEPLAY );
	// Manager-owned, page-granular recyclable metadata in the same reservation.
	void *allocateRecyclableMetadataPage( sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_GAMEPLAY );
	bool releaseRecyclableMetadataPage( void *pPage, sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_GAMEPLAY );

	sirMemoryStats_t getStats( void ) const;
	bool initTraceBuffer( uint32_t nEntries );
	uint32_t copyTrace( sirMemoryTraceEvent_t *pOutput, uint32_t nCapacity ) const;
	uint64_t getPageSize( void ) const;

private:
	friend class SIRMemorySegment;

	bool commit( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes );
	bool decommit( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes );
	uint64_t getSegmentCommittedBytes( const SIRMemorySegment *pSegment ) const;
	void *allocateMetadataLocked( uint64_t nBytes, uint64_t nAlignment, sirMemoryDomain_t eDomain );
	uint64_t countCommittedPages( const SIRMemorySegment *pSegment, uint64_t nFirstPage, uint64_t nPageCount ) const;
	void recordTraceLocked( uint32_t nType, uint64_t nBytes, uint32_t nSegment );
	void markCommittedPages( SIRMemorySegment *pSegment, uint64_t nFirstPage, uint64_t nPageCount, bool bCommitted );

	mutable SIRMutex m_Mutex;
	uint8_t *m_pBase;
	SIRMemorySegment *m_pSegments;

	uint64_t m_nPageSize;
	uint32_t m_nPageShift;
	uint64_t m_nReservedBytes;
	uint64_t m_nCommitBudgetBytes;
	uint64_t m_nMaximumCommitBudgetBytes;
	uint64_t m_nCommitDeniedCount;
	uint64_t m_nCommitCallCount;
	uint64_t m_nDecommitCallCount;
	uint64_t m_nDecommittedBytes;
	uint64_t m_nTraceDroppedCount;
	uint64_t m_nTraceSequence;
	sirMemoryTraceEvent_t *m_pTrace;
	uint32_t m_nTraceCapacity;
	uint32_t m_nTraceCount;
	uint64_t m_nDomainCommittedBytes[SIR_MEMORY_DOMAIN_COUNT];
	uint64_t m_nDomainMetadataCommittedBytes[SIR_MEMORY_DOMAIN_COUNT];
	uint64_t m_nDomainPeakCommittedBytes[SIR_MEMORY_DOMAIN_COUNT];
	uint64_t m_nDomainCommitLimits[SIR_MEMORY_DOMAIN_COUNT];
	uint64_t m_nCommittedBytes;
	uint64_t m_nPeakCommittedBytes;
	uint64_t m_nMetadataBytes;
	uint64_t m_nMetadataCommittedBytes;
	uint64_t m_nMetadataOffset;
	uint64_t m_nRecyclableFloor;
	uint64_t m_nRecyclableCommittedBytes;
	uint64_t m_nRecyclableUsedBytes;
	uint8_t *m_pRecyclableStates;
	uint64_t m_nSegmentCursor;
	uint32_t m_nMaxSegments;
	uint32_t m_nSegmentCount;
};

#endif // __SIRENGINE_SIRMEMORYMANAGER_H__

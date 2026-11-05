/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRCOMPACTBUCKETALLOCATOR_H__
#define __SIRENGINE_SIRCOMPACTBUCKETALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRAtomic.h>
#include <cstdint>

class SIRMemorySegment;

typedef struct sirCompactBucketConfig_s
{
	uint32_t nMaximumSmallBytes;
	uint32_t nAlignment;
	uint32_t nSlabBytes;
	uint32_t nWorkerSeats;
	uint32_t nWarmSlabsPerSeat;
	uint32_t nClassProfile = 0; // 0=40, 1=8, 2=24, 3=32 (experimental)
} sirCompactBucketConfig_t;

// One segment shared by all size classes and worker seats. No CRT-backed
// allocator metadata. Ordinary local allocation and free are owner-only.
constexpr uint32_t SIR_BUCKET_NONE = UINT32_MAX;

typedef struct sirBucketSlab_s
{
	SIRAtomic<uint32_t> nClass;
	SIRAtomic<uint32_t> nFresh;
	SIRAtomic<uint32_t> nAccessGate;
	SIRAtomic<uint32_t> nSeat;
	SIRAtomic<uint32_t> nSeatGeneration;
	uint32_t nLive;
	uint32_t nFreeHead;
	uint32_t nNeutralNext;
	uint32_t nLastClass;
	uint32_t nPartialNext;
	uint32_t nPartialPrev;
	SIRAtomic<uint32_t> nEpoch;
	SIRAtomic<uint64_t> *pStates;
	SIRAtomic<uint64_t> *pDirtyStateWords;
	bool bInPartial;
	bool bCommitted;
	bool bWarmRetained;
	sirBucketSlab_s( void );
} sirBucketSlab_t;

typedef struct sirBucketStatePage_s
{
	sirBucketStatePage_s *pNext;
	uint8_t *pBase;
	uint64_t nFreeBits;
	uint32_t nUsed;
	uint32_t nSlots;
} sirBucketStatePage_t;

typedef struct sirBucketDirectory_s
{
	SIRAtomic<sirBucketSlab_t *> pDescriptor;
	SIRAtomic<uint32_t> nReaders;
	SIRAtomic<uint32_t> nEpoch;
	sirBucketDirectory_s( void );
} sirBucketDirectory_t;

typedef struct alignas( 64 ) sirBucketInbox_s
{
	SIRAtomic<uint64_t> *pDirtySlabWords;
	SIRAtomic<uint64_t> *pDirtyGroupWords;
	sirBucketInbox_s( void );
} sirBucketInbox_t;

typedef struct sirBucketSeat_s
{
	SIRAtomic<uintptr_t> nOwnerToken;
	SIRAtomic<uint32_t> nGeneration;
	SIRAtomic<uint32_t> nOrphan;
	SIRAtomic<uint64_t> nLive;
	uint64_t nLiveOwner;    // Single-writer live count while the seat is attached.
	uint32_t nWarmRetained; // Owner-only number of empty live-class slabs retained.
	uint32_t *pActive;
	uint32_t *pPartialHead;
	sirBucketSlab_t **pActiveDescriptor;
	sirBucketInbox_t *pRemote;
	sirBucketSeat_s( void );
} sirBucketSeat_t;

typedef struct sirSlotLocation_s
{
	uint32_t nSlab;
	uint32_t nSlot;
	uint32_t nClass;
	uint32_t nEpoch;
} sirSlotLocation_t;

class SIRCompactBucketAllocator
{
public:
	SIRCompactBucketAllocator( void );
	~SIRCompactBucketAllocator( void );
	static bool retireRegistered( SIRCompactBucketAllocator *pAllocator );
	SIRCompactBucketAllocator( const SIRCompactBucketAllocator& ) = delete;
	SIRCompactBucketAllocator& operator=( const SIRCompactBucketAllocator& ) = delete;

	static uint64_t getRequiredMetadataBytes( uint64_t nSegmentBytes, const sirCompactBucketConfig_t& config );
	bool init( SIRMemorySegment *pSegment, const sirCompactBucketConfig_t& config );
	bool attach( uint32_t *pSeat = nullptr );
	bool detach( void );
	bool retireThread( void );
	uint32_t serviceOrphans( uint32_t nMaximum = 128 );
	bool hasPendingOrphans( void ) const;
	uint32_t getAttachedSeat( void ) const;
	void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
	bool free( void *pMemory );
	uint32_t getUsableSize( const void *pMemory ) const;
	uint32_t getMaximumBytes( void ) const;
	uint32_t getMinimumAlignment( void ) const;
	bool owns( const void *pMemory ) const;
	uint32_t drainRemote( void );
	uint32_t servicePending( uint32_t nMaximum );
	uint64_t trimEmptySlabs( uint32_t nMaximumWarm );
	uint64_t trimIdle( void );
	uint64_t getLiveCount( void ) const;
	bool shutdown( void );

private:
	static constexpr uint32_t SIR_BUCKET_RETIRED = 0x80000000u;
	static constexpr uint32_t SIR_BUCKET_CLASSES = 40;

	// Stable slab-ID directory. Descriptor payloads are instantiated only
	// when a reserved slab first becomes active; the pointer remains stable
	// until manager shutdown. Reclamation of descriptor payloads is O06.

	bool resolve( const void *pMemory, sirSlotLocation_t *pLocation ) const;
	sirBucketSlab_t *loadSlab( uint32_t nSlab ) const;
	bool enterProducer( uint32_t nSlab, uint32_t nEpoch );
	void exitProducer( uint32_t nSlab );
	bool fillActive( uint32_t nSeat, uint32_t nClass );
	bool findSeat( uint32_t *pSeat ) const;
	void releaseSlot( uint32_t nSlab, uint32_t nSlot );
	void makeNeutral( uint32_t nSlab );
	void linkPartial( uint32_t nSeat, uint32_t nClass, uint32_t nSlab );
	void unlinkPartial( uint32_t nSeat, uint32_t nClass, uint32_t nSlab );
	uint32_t drainSeat( uint32_t nSeat, uint32_t nMaximum = UINT32_MAX );
	void publishDirtySlab( uint32_t nSeat, uint32_t nSlab );
	uint32_t drainDirtySlab( uint32_t nSeat, uint32_t nSlab, uint32_t nMaximum );
	uint32_t getSlotCapacity( uint32_t nClass ) const;
	uint32_t loadState( uint32_t nSlab, uint32_t nSlot ) const;
	bool claimState( uint32_t nSlab, uint32_t nSlot, uint32_t nExpected, uint32_t nDesired );
	SIRAtomic<uint64_t> *allocateStateBlock( void );
	void releaseStateBlock( SIRAtomic<uint64_t> *pBlock );

	SIRCompactBucketAllocator *m_pRegistryNext;
	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	sirBucketDirectory_t *m_pDirectory;
	sirBucketSeat_t *m_pSeats;
	sirBucketInbox_t *m_pRemoteInboxes;
	SIRAtomic<uint64_t> *m_pFreeStateBlocks;
	sirBucketStatePage_t *m_pStatePages;
	uint32_t m_nStateBlockStride;
	uint32_t m_nStateSlotsPerPage;
	uint32_t m_nStateWordsPerSlab;
	uint32_t m_nDirtyStateWordsPerSlab;
	uint32_t m_nDirtySlabWordsPerSeat;
	uint32_t m_nDirtyGroupsPerSeat;
	// Class strides are bounded by the configured 2,048-byte small-object limit.
	uint16_t m_nClassStride[SIR_BUCKET_CLASSES];
	uint32_t m_nClassCapacity[SIR_BUCKET_CLASSES];
	uint32_t m_nClassReciprocal[SIR_BUCKET_CLASSES];
	uint32_t m_nAlignmentShift;
	uint8_t m_nClassMap[256];
	uint32_t m_nClassCount;
	uint32_t m_nMapCount;
	uint32_t m_nMaxBytes;
	uint32_t m_nAlignment;
	uint32_t m_nSlabBytes;
	uint32_t m_nSlabShift;
	uint32_t m_nSlabCount;
	uint32_t m_nMaximumSlots;
	uint32_t m_nSeatCount;
	uint32_t m_nWarmPerSeat;
	SIRAtomic<uint32_t> m_nNextDormant;
	SIRAtomic<uint32_t> m_nOrphanCount;
	uint32_t m_nNeutralHead;
	SIRMutex m_ColdMutex;
	SIRMutex m_OrphanMutex;
};

#endif // __SIRENGINE_SIRCOMPACTBUCKETALLOCATOR_H__

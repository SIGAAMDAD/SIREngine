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

#ifndef __SIRENGINE_SIRSLABALLOCATOR_H__
#define __SIRENGINE_SIRSLABALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// Fixed-slot slabs with out-of-band descriptors and explicit empty-slab trim.
// One allocator owns one exclusive segment. No malloc or virtual dispatch.
typedef enum sirSlabState_e : uint32_t
{
	SIR_SLAB_DORMANT = 0,
	SIR_SLAB_EMPTY,
	SIR_SLAB_PARTIAL,
	SIR_SLAB_ACTIVE, // Cached allocation slab, intentionally off global lists.
	SIR_SLAB_FULL,
	SIR_SLAB_DECOMMITTED
} sirSlabState_t;

typedef struct sirSlabDesc_s
{
	uint32_t nNext;
	uint32_t nPrev;
	uint32_t nFreeHead;
	uint32_t nFresh;
	uint32_t nLive;
	uint32_t nState;
} sirSlabDesc_t;

class SIRSlabAllocator
{
public:
	SIRSlabAllocator( void );
	SIRSlabAllocator( const SIRSlabAllocator& ) = delete;
	SIRSlabAllocator& operator=( const SIRSlabAllocator& ) = delete;

	static uint64_t getRequiredMetadataBytes( uint64_t nSegmentBytes, uint64_t nSlabBytes, uint32_t nObjectBytes = 16,
	                                          uint32_t nAlignment = 16 );
	bool init( SIRMemorySegment *pSegment, uint32_t nObjectBytes, uint32_t nAlignment = 16, uint64_t nSlabBytes = 16384 );
	void *alloc( void );
	bool free( void *pMemory );
	bool precommitSlabs( uint32_t nSlabCount );
	bool trimEmptySlabs( void );
	bool trimEmptySlabsTo( uint32_t nKeepWarm );
	bool owns( const void *pMemory ) const;
	bool containsSlotAddress( const void *pMemory ) const;
	bool getSlotIndex( const void *pMemory, uint64_t *pIndex ) const;
	uint64_t getSlotIndexCapacity( void ) const;
	uint64_t getColdActivations( void ) const;
	bool shutdown( void );

	uint32_t getLiveSlots( void ) const;
	uint32_t getSlotsPerSlab( void ) const;
	uint32_t getSlabCount( void ) const;
	uint32_t getCommittedSlabs( void ) const;

private:
	static constexpr uint32_t SIR_SLAB_NONE = UINT32_MAX;

	void unlink( uint32_t nIndex );
	void link( uint32_t nIndex, uint32_t nState );
	bool activateSlab( uint32_t nIndex );
	uint8_t *slot( uint32_t nSlab, uint32_t nSlot ) const;

	SIRMemorySegment *m_pSegment;
	sirSlabDesc_t *m_pDescs;
	uint8_t *m_pBase;
	uint64_t m_nSlabBytes;
	uint32_t m_nStride;
	uint32_t m_nSlabShift;
	uint32_t m_nStrideShift;
	uint32_t m_nSlabCount;
	uint32_t m_nSlotsPerSlab;
	uint32_t m_nNextDormant;
	uint32_t m_nActiveSlab;
	uint32_t m_nCommittedSlabs;
	uint64_t m_nColdActivations;
	uint32_t m_nLiveSlots;
	uint32_t m_nEmptyHead;
	uint32_t m_nPartialHead;
	uint32_t m_nDormantHead;
	uint64_t *m_pLiveBits;
};

#endif // __SIRENGINE_SIRSLABALLOCATOR_H__

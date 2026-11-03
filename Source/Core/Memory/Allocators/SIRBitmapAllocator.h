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

#ifndef __SIRENGINE_SIRBITMAPALLOCATOR_H__
#define __SIRENGINE_SIRBITMAPALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// Fixed-stride allocation with out-of-band hierarchical free-bitmaps.
// Payload pages are committed as a contiguous prefix on demand.
// No intrusive writes occur in freed payload slots.
class SIRBitmapAllocator
{
public:
	SIRBitmapAllocator( void );
	SIRBitmapAllocator( const SIRBitmapAllocator & ) = delete;
	SIRBitmapAllocator &operator=( const SIRBitmapAllocator & ) = delete;

	static uint64_t getRequiredMetadataBytes( uint64_t nSegmentBytes, uint32_t nObjectBytes, uint32_t nAlignment = 16 );
	bool init( SIRMemorySegment *pSegment, uint32_t nObjectBytes, uint32_t nAlignment = 16, uint64_t nCommitIncrementBytes = 0 );
	void *alloc( void );
	bool free( void *pMemory );
	bool precommitSlots( uint32_t nSlots );
	bool trimUnusedPages( void );
	bool shutdown( void );

	uint32_t getCapacitySlots( void ) const;
	uint32_t getActiveSlots( void ) const;
	uint32_t getLiveSlots( void ) const;
	uint32_t getSlotStride( void ) const;

private:
	static constexpr uint32_t SIR_BITMAP_LEVELS = 6;
	static uint32_t findSetBit( uint64_t nWord );
	static uint32_t highestSetBit( uint64_t nWord );
	void updateWordSummary( uint32_t nWordIndex, bool bAvailable );
	void changeFreeRange( uint32_t nBegin, uint32_t nEnd, bool bSetFree );
	void setFree( uint32_t nSlot );
	bool isFree( uint32_t nSlot ) const;
	bool grow( uint32_t nMinimumSlots );

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	uint64_t *m_pLevels[SIR_BITMAP_LEVELS];
	uint32_t m_nLevelWords[SIR_BITMAP_LEVELS];
	uint64_t m_nCommittedBytes;
	uint64_t m_nCommitIncrementBytes;
	uint32_t m_nLevels;
	uint32_t m_nStride;
	uint32_t m_nCapacitySlots;
	uint32_t m_nActiveSlots;
	uint32_t m_nFreeSlots;
	uint32_t m_nLiveSlots;
	uint32_t m_nHotWord;
};

#endif // __SIRENGINE_SIRBITMAPALLOCATOR_H__

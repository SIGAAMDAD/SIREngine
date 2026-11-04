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

#ifndef __SIRENGINE_SIRPOOLALLOCATOR_H__
#define __SIRENGINE_SIRPOOLALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// Fixed-size, single-owner pool. free slots contain an intrusive 32-bit
// next-free index. Occupied slots contain no allocation header.
// Commitment advances by a contiguous prefix; freed slots remain warm.
// release builds require well-formed single frees by the owning caller.
class SIRPoolAllocator
{
public:
	SIRPoolAllocator( void );
	SIRPoolAllocator( const SIRPoolAllocator& ) = delete;
	SIRPoolAllocator& operator=( const SIRPoolAllocator& ) = delete;
	SIRPoolAllocator( SIRPoolAllocator&& ) = delete;
	SIRPoolAllocator& operator=( SIRPoolAllocator&& ) = delete;

	bool init( SIRMemorySegment *pSegment, uint32_t nObjectBytes, uint32_t nAlignment = 16, uint64_t nCommitIncrementBytes = 0 );
	void *alloc( void );
	bool free( void *pMemory );

	// Cold operations. reset/shutdown require no live objects.
	bool precommitSlots( uint32_t nSlots );
	bool reset( bool bDecommit = false );
	bool shutdown( void );

	uint32_t getObjectBytes( void ) const;
	uint32_t getSlotStride( void ) const;
	uint32_t getCapacitySlots( void ) const;
	uint32_t getLiveSlots( void ) const;
	uint32_t getFreshSlots( void ) const;
	uint64_t getCommittedPrefixBytes( void ) const;

private:
	static constexpr uint32_t SIR_POOL_INVALID_SLOT = UINT32_MAX;

	bool commitToSlotCount( uint32_t nSlots );
	uint8_t *slotAddress( uint32_t nIndex ) const;

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	// Base is the first aligned slot; no separate first-offset hot field.
	// High bit of magic selects the power-of-two shift case.
	uint64_t m_nStrideMagic;
	uint64_t m_nCommittedPrefixBytes;
	uint64_t m_nCommitIncrementBytes;
	uint32_t m_nObjectBytes;
	uint32_t m_nSlotStride;
	uint32_t m_nCapacitySlots;
	uint32_t m_nFreshIndex;
	uint32_t m_nFreeHead;
	uint32_t m_nLiveCount;
	uint64_t *m_pLiveBits;
	uint64_t m_nLiveBitmapWords;
};

#endif // __SIRENGINE_SIRPOOLALLOCATOR_H__

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

#ifndef __SIRENGINE_SIRFREELISTALLOCATOR_H__
#define __SIRENGINE_SIRFREELISTALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRHeapBlock.h>
#include <cstdint>

class SIRMemorySegment;

// Variable-size segregated free-list heap with in-band boundary tags.
// Segment payload backing grows as one contiguous page-aligned prefix.
class SIRFreeListAllocator
{
public:
	SIRFreeListAllocator( void );
	SIRFreeListAllocator( const SIRFreeListAllocator& ) = delete;
	SIRFreeListAllocator& operator=( const SIRFreeListAllocator& ) = delete;

	bool init( SIRMemorySegment *pSegment, uint32_t nCommitIncrementBytes = 65536 );
	void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
	bool free( void *pMemory );
	uint32_t getUsableSize( const void *pMemory ) const;
	bool trimUnusedPages( void );
	bool shutdown( void );

	uint32_t getAllocationCount( void ) const;
	uint32_t getCommittedBytes( void ) const;
	uint32_t getAllocatedBlockBytes( void ) const;

private:
	sirHeapBlock_t *block( uint32_t nOffset ) const;
	static constexpr uint32_t SIR_FREE_CLASSES = 64;

	static uint32_t classForSize( uint32_t nBytes );
	static uint32_t lowestSetBit( uint64_t nMask );
	void insertFree( uint32_t nOffset );
	void removeFree( uint32_t nOffset );
	void removeFreeAtClass( uint32_t nOffset, uint32_t nClass );
	void updateNextPrev( uint32_t nOffset );
	bool grow( uint32_t nMinimumBytes );
	bool requiredBytes( uint32_t nBytes, uint32_t nAlignment, uint32_t *pBytes ) const;
	uint32_t usedBytes( uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset ) const;
	void *allocateBlock( uint32_t nOffset, uint32_t nUsed, uint32_t nUserOffset, uint32_t nClass );

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	uint32_t m_nCommittedBytes;
	uint32_t m_nCapacityBytes;
	uint32_t m_nIncrementBytes;
	uint64_t m_nNonemptyClasses;
	uint32_t m_nFreeHeads[SIR_FREE_CLASSES];
	uint32_t m_nLastBlock;
	uint32_t m_nAllocatedBlockBytes;
	uint32_t m_nAllocationCount;
};

#endif // __SIRENGINE_SIRFREELISTALLOCATOR_H__

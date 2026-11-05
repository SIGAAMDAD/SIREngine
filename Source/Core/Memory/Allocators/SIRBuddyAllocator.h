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

#ifndef __SIRENGINE_SIRBUDDYALLOCATOR_H__
#define __SIRENGINE_SIRBUDDYALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// Page-granularity buddy allocator. A single power-of-two segment is partitioned
// into power-of-two blocks. Allocation-tree metadata is in manager metadata
// (part of the same OS reservation), never in decommittable data pages.
// free() retains backing for quick reuse. trimUnusedPages() is an explicit cold
// operation and only decommits completely free block subtrees.
class SIRBuddyAllocator
{
public:
	SIRBuddyAllocator( void );
	SIRBuddyAllocator( const SIRBuddyAllocator& ) = delete;
	SIRBuddyAllocator& operator=( const SIRBuddyAllocator& ) = delete;

	static uint64_t getRequiredMetadataBytes( uint64_t nCapacityBytes, uint64_t nMinimumBlockBytes );

	bool init( SIRMemorySegment *pSegment, uint64_t nMinimumBlockBytes = 0 );
	void *alloc( uint64_t nBytes, uint64_t nAlignment = 16 );
	bool free( void *pMemory );
	uint64_t getUsableSize( const void *pMemory ) const;
	bool trimUnusedPages( void );

	uint64_t getAllocatedBytes( void ) const;
	uint32_t getAllocationCount( void ) const;
	uint64_t getMinimumBlockBytes( void ) const;
	uint64_t getCapacity( void ) const;

private:
	static constexpr uint8_t SIR_BUDDY_FULL = 0xff;
	static constexpr uint8_t SIR_COMMIT_NONE = 0;
	static constexpr uint8_t SIR_COMMIT_FULL = 1;
	static constexpr uint8_t SIR_COMMIT_MIXED = 2;

	uint8_t readFreeOrder( uint32_t nNode ) const;
	void writeFreeOrder( uint32_t nNode, uint8_t nOrder );
	void updateParent( uint32_t nNode, uint8_t nChildOrder );
	bool trimNode( uint32_t nNode, uint8_t nOrder, uint64_t nOffset );
	uint8_t readCommitState( uint32_t nNode ) const;
	void writeCommitState( uint32_t nNode, uint8_t nState );
	uint8_t effectiveCommitState( uint32_t nNode ) const;
	void setCommitState( uint32_t nNode, uint8_t nState );
	void setLeafCommitRange( uint32_t nFirst, uint32_t nCount, bool bCommitted );

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pFreeOrders;
	uint8_t *m_pAllocatedOrders;
	uint8_t *m_pCommitTree;
	uint64_t *m_pCommittedLeafBits;
	uint64_t m_nMinBlockBytes;
	uint64_t m_nAllocatedBytes;
	uint32_t m_nLeafCount;
	uint32_t m_nAllocationCount;
	uint8_t m_nRootOrder;
};

#endif // __SIRENGINE_SIRBUDDYALLOCATOR_H__

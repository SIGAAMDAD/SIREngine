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

#ifndef __SIRENGINE_SIRHANDLEHEAP_H__
#define __SIRENGINE_SIRHANDLEHEAP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRMemorySegment;

typedef struct sirMemoryHandle_s
{
	uint32_t nIndex;
	uint32_t nGeneration;
} sirMemoryHandle_t;
static_assert( sizeof( sirMemoryHandle_t ) == 8, "8-byte memory handle" );

// Movable raw-byte allocations with stable generation-checked handles.
// Explicit cold compaction memmoves only unpinned payloads.
// No automatic C++ construction/destruction or pointer fixups.
typedef struct sirHandleDesc_s
{
	uint32_t nBlockOffset;
	uint32_t nUserOffset;
	uint32_t nGeneration;
	uint32_t nStatePin; // Bit 0 live; bits 1..16 pin count.
} sirHandleDesc_t;

typedef struct sirHandleCold_s
{
	uint32_t nBlockBytes;
	uint32_t nPayloadBytes;
	uint32_t nAlignment;
	uint32_t nNextFree;
} sirHandleCold_t;

typedef struct sirTreeLinks_s
{
	uint32_t nLeft;
	uint32_t nRight;
	uint32_t nParent;
	uint8_t nHeight;
	uint8_t nReserved[3];
} sirTreeLinks_t;

typedef struct sirHandleHeapExtent_s
{
	uint32_t nOffset;
	uint32_t nBytes;
	// Recycled extents store the free-list link in address.nLeft.
	// The address tree is inactive while this descriptor is recycled.
	sirTreeLinks_t address;
	sirTreeLinks_t size;
} sirHandleHeapExtent_t;

class SIRHandleHeap
{
public:
	SIRHandleHeap( void );
	SIRHandleHeap( const SIRHandleHeap & ) = delete;
	SIRHandleHeap &operator=( const SIRHandleHeap & ) = delete;

	static uint64_t getRequiredMetadataBytes( uint32_t nMaxHandles );
	bool init( SIRMemorySegment *pSegment, uint32_t nMaxHandles );
	void *alloc( uint32_t nBytes, uint32_t nAlignment, sirMemoryHandle_t *pHandle );
	void *resolve( sirMemoryHandle_t handle ) const;
	void *pin( sirMemoryHandle_t handle );
	bool unpin( sirMemoryHandle_t handle );
	bool free( sirMemoryHandle_t handle );
	bool compact( void );
	uint64_t getCompactionPayloadUpperBound( void ) const;
	bool compactWithinBudget( uint64_t nMaximumPayloadBytes );
	bool trimUnusedPages( void );
	bool shutdown( void );

	uint32_t getLiveCount( void ) const;
	uint32_t getLargestFreeRange( void ) const;
	uint32_t getCommittedBytes( void ) const;

private:
	// Resolve consumes only one 16-byte cache-local descriptor.

	static_assert( sizeof( sirHandleDesc_t ) == 16, "16-byte hot handle descriptors" );
	static_assert( sizeof( sirHandleCold_t ) == 16, "16-byte cold handle descriptors" );
	static_assert( sizeof( sirHandleHeapExtent_t ) == 40, "40-byte dual-index extents" );
	static constexpr uint32_t SIR_HANDLE_NONE = UINT32_MAX;

	sirHandleDesc_t *validate( sirMemoryHandle_t handle ) const;
	bool acquireHandle( uint32_t *pIndex );
	void recycleHandle( uint32_t nIndex );
	uint32_t acquireExtent( void );
	void recycleExtent( uint32_t nIndex );
	bool insertExtent( uint32_t nOffset, uint32_t nBytes );
	void removeExtent( uint32_t nIndex );
	uint32_t findFittingExtent( uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset, uint32_t *pBlockBytes ) const;

	template <bool bSizeTree>
	sirTreeLinks_t &getLinks( uint32_t nIndex );
	template <bool bSizeTree>
	const sirTreeLinks_t &getLinks( uint32_t nIndex ) const;
	template <bool bSizeTree>
	uint32_t &root( void );
	template <bool bSizeTree>
	uint32_t rootValue( void ) const;
	template <bool bSizeTree>
	bool treeLess( uint32_t nLeft, uint32_t nRight ) const;
	template <bool bSizeTree>
	uint8_t treeHeight( uint32_t nNode ) const;
	template <bool bSizeTree>
	void updateHeight( uint32_t nNode );
	template <bool bSizeTree>
	uint32_t rotateLeft( uint32_t nNode );
	template <bool bSizeTree>
	uint32_t rotateRight( uint32_t nNode );
	template <bool bSizeTree>
	void rebalance( uint32_t nNode );
	template <bool bSizeTree>
	void replaceNode( uint32_t nNode, uint32_t nReplacement );
	template <bool bSizeTree>
	void treeInsert( uint32_t nIndex );
	template <bool bSizeTree>
	void treeRemove( uint32_t nIndex );
	template <bool bSizeTree>
	uint32_t treeFirst( uint32_t nNode ) const;
	template <bool bSizeTree>
	uint32_t treeLast( uint32_t nNode ) const;
	template <bool bSizeTree>
	uint32_t treeNext( uint32_t nIndex ) const;
	void trimLogicalTail( void );
	bool prepareBlock( uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset, uint32_t *pBlockBytes ) const;
	uint32_t sortActive( void );
	uint32_t mergeSortedRuns( uint32_t nLeft, uint32_t nRight );
	bool less( uint32_t nLeft, uint32_t nRight ) const;

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	sirHandleDesc_t *m_pHandles;
	sirHandleCold_t *m_pCold;
	sirHandleHeapExtent_t *m_pExtents;
	uint32_t m_nCapacity;
	uint32_t m_nCommitted;
	uint32_t m_nHighWater;
	uint32_t m_nMaxHandles;
	uint32_t m_nNextHandle;
	uint32_t m_nFreeHandleHead;
	uint32_t m_nLiveCount;
	uint32_t m_nExtentCapacity;
	uint32_t m_nNextExtent;
	uint32_t m_nFreeExtentHead;
	uint32_t m_nAddressRoot;
	uint32_t m_nSizeRoot;
};

#endif // __SIRENGINE_SIRHANDLEHEAP_H__

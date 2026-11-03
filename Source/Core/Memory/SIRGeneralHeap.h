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

#ifndef __SIRENGINE_SIRGENERALHEAP_H__
#define __SIRENGINE_SIRGENERALHEAP_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRSync.h>
#include <Core/Memory/SIRMemoryManager.h>

class SIRCompactBucketAllocator;
class SIRFreeListAllocator;
class SIRTLSFAllocator;
class SIRBuddyAllocator;
class SIRPageRunAllocator;
class SIRMemorySegment;

// All backends own disjoint, permanently assigned manager segments. Only the
// compact bucket path has worker-local state; medium and large heaps are
// serialized at this facade, never called concurrently without their mutex.
typedef struct sirGeneralHeapConfig_s
{
	SIRCompactBucketAllocator *pSmall;
	SIRFreeListAllocator *pFreeList;
	SIRTLSFAllocator *pTLSF;
	SIRBuddyAllocator *pLarge;
	SIRMemorySegment *pSmallSegment;
	SIRMemorySegment *pMediumSegment;
	SIRMemorySegment *pLargeSegment;
	uint32_t nSmallMaximumBytes;
	uint32_t nLargeMinimumBytes;
	bool bUseTLSF;
	SIRPageRunAllocator *pPageRuns = nullptr;
	bool bUsePageRuns = false;
} sirGeneralHeapConfig_t;

typedef enum sirHeapRoute_e : uint32_t
{
	SIR_HEAP_NONE,
	SIR_HEAP_SMALL,
	SIR_HEAP_MEDIUM,
	SIR_HEAP_LARGE
} sirHeapRoute_t;

class SIRGeneralHeap
{
public:
	SIRGeneralHeap( void );
	SIRGeneralHeap( const SIRGeneralHeap & ) = delete;
	SIRGeneralHeap &operator=( const SIRGeneralHeap & ) = delete;

	bool init( const sirGeneralHeapConfig_t &config );
	bool isConfiguredForDomain( sirMemoryDomain_t eDomain ) const noexcept;
	// Worker entry/exit can be explicitly tied to the engine worker lifecycle.
	bool attachThread( void );
	bool detachThread( void );
	void *alloc( size_t nBytes );
	void *allocAligned( size_t nBytes, size_t nAlignment );
	void *allocZeroed( size_t nCount, size_t nBytes );
	void *realloc( void *pMemory, size_t nNewBytes );
	void *reallocAligned( void *pMemory, size_t nNewBytes, size_t nAlignment );
	void free( void *pMemory );
	bool tryFree( void *pMemory );
	size_t getUsableSize( const void *pMemory ) const;
	bool owns( const void *pMemory ) const;
	uint32_t drainRemote( void );
	uint32_t serviceThread( uint32_t nMaximum = 128 );
	bool shutdown( void );

private:
	sirHeapRoute_t routePointer( const void *pMemory ) const;
	void *allocRaw( size_t nBytes, size_t nAlignment );
	static bool contains( const SIRMemorySegment *pSegment, const void *pMemory );
	static bool overlap( const SIRMemorySegment *pA, const SIRMemorySegment *pB );

	sirGeneralHeapConfig_t m_Config;
	mutable SIRMutex m_MediumMutex;
	mutable SIRMutex m_LargeMutex;
	bool m_bInitialized;
};

#endif // __SIRENGINE_SIRGENERALHEAP_H__

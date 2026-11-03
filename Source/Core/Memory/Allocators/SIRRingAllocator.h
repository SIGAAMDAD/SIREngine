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

#ifndef __SIRENGINE_SIRRINGALLOCATOR_H__
#define __SIRENGINE_SIRRINGALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// FIFO circular allocator with monotonic retirement sequences.
// Payload pages commit on demand as a monotonic prefix; precommit() warms all pages.
typedef struct sirRingEntry_s
{
	uint64_t nEnd; // Sequence derives from the FIFO's head and count.
} sirRingEntry_t;

class SIRRingAllocator
{
public:
	SIRRingAllocator( void );
	SIRRingAllocator( const SIRRingAllocator & ) = delete;
	SIRRingAllocator &operator=( const SIRRingAllocator & ) = delete;

	static uint64_t getRequiredMetadataBytes( uint32_t nMaxAllocations );
	bool init( SIRMemorySegment *pSegment, uint32_t nMaxAllocations );
	bool precommit( void );
	void *alloc( uint32_t nBytes, uint32_t nAlignment, uint64_t *pSequence );
	bool retireThrough( uint64_t nSequence );
	bool trimIfEmpty( void );
	bool shutdown( void );

	uint64_t getFreeBytes( void ) const;
	uint64_t getLastSequence( void ) const;
	uint32_t getOutstandingCount( void ) const;

private:
	SIRMemorySegment *m_pSegment;
	sirRingEntry_t *m_pEntries;
	uint8_t *m_pBase;
	uint64_t m_nCapacity;
	uint64_t m_nHead;
	uint64_t m_nTail;
	uint64_t m_nNextSequence;
	uint64_t m_nRetiredSequence;
	uint64_t m_nCapacityMask;
	uint64_t m_nCommittedPrefixBytes;
	uint32_t m_nEntryCapacity;
	uint32_t m_nEntryHead;
	uint32_t m_nEntryTail;
	uint32_t m_nCount;
	bool commitThrough( uint64_t nEnd );
	uint64_t wrapOffset( uint64_t nOffset ) const;
};

#endif // __SIRENGINE_SIRRINGALLOCATOR_H__

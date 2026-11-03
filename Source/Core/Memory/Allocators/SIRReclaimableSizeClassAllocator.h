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

#ifndef __SIRENGINE_SIRRECLAIMABLESIZECLASSALLOCATOR_H__
#define __SIRENGINE_SIRRECLAIMABLESIZECLASSALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <cstdint>

class SIRMemorySegment;

// Opt-in slab composite; original pool-backed size-class implementation stays intact.
// The caller allocates the segment layout before Init. No runtime createSegment.
typedef struct sirClassState_s
{
	uint64_t nLastActivations;
	uint32_t nWarmSlabs;
	uint32_t nQuietEpochs;
} sirClassState_t;

class SIRReclaimableSizeClassAllocator
{
public:
	SIRReclaimableSizeClassAllocator( void );
	bool init( SIRMemorySegment *const *ppSegments, bool bFine = false, uint64_t nSlabBytes = 16384, uint32_t nAlignment = 16 );
	void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
	bool free( void *pMemory, uint32_t nBytes );
	bool precommitClass( uint32_t nClass, uint32_t nSlabs );
	bool trimIdle( void );
	bool trimTo( uint32_t nMaximumWarmSlabs );
	bool setWarmSlabs( uint32_t nClass, uint32_t nWarm );
	bool setAdaptiveRetention( bool bEnabled );
	bool advanceRetentionEpoch( void );
	bool shutdown( void );
	uint32_t getClassCount( void ) const;
	uint64_t getLiveRequestedBytes( void ) const;
	uint64_t getClassCommittedBytes( uint32_t nClass ) const;

private:
	SIRSlabAllocator *m_pSlabs;
	SIRMemorySegment **m_ppSegments;
	sirClassState_t *m_pClassState;
	uint32_t m_nClassCount;
	uint32_t m_nInitialized;
	uint32_t m_nAlignment;
	uint64_t m_nLiveRequestedBytes;
	bool m_bFine;
	bool m_bAdaptive;
};

#endif // __SIRENGINE_SIRRECLAIMABLESIZECLASSALLOCATOR_H__

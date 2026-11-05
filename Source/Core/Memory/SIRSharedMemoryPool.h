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

#ifndef __SIRENGINE_SIRSHAREDMEMORYPOOL_H__
#define __SIRENGINE_SIRSHAREDMEMORYPOOL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRMemorySegment;

typedef struct sirSharedMemoryRegion_s
{
	SIRMemorySegment *pSegment;
	uint64_t nOffsetBytes;
	uint64_t nReservedBytes;
} sirSharedMemoryRegion_t;

// Startup-only region carving from ONE manager segment. Every region starts
// and ends at an OS-page boundary; clients own different pages, not segments.
// No implicit heap fallback or individually releasable reservations.
class SIRSharedMemoryPool
{
public:
	SIRSharedMemoryPool( void );
	bool init( SIRMemorySegment *pSegment );
	bool allocateRegion( uint64_t nBytes, sirSharedMemoryRegion_t& region );
	// Cold rollback only; the caller must release all borrowers first.
	bool releaseLastRegion( const sirSharedMemoryRegion_t& region );
	uint64_t getAvailableBytes( void ) const;
	SIRMemorySegment *getSegment( void ) const;

	static void *getBase( const sirSharedMemoryRegion_t& region );
	static bool commit( const sirSharedMemoryRegion_t& region, uint64_t nOffset, uint64_t nBytes );
	static bool decommit( const sirSharedMemoryRegion_t& region, uint64_t nOffset, uint64_t nBytes );

private:
	SIRMemorySegment *m_pSegment;
	uint64_t m_nCursor;
};

#endif // __SIRENGINE_SIRSHAREDMEMORYPOOL_H__

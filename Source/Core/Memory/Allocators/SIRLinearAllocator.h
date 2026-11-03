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

#ifndef __SIRENGINE_SIRLINEARALLOCATOR_H__
#define __SIRENGINE_SIRLINEARALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// One-owner bump allocator. Does not call the general-purpose heap.
// Marks rewind logical allocations; trimTo() returns unused trailing pages.
class SIRLinearAllocator
{
public:
	SIRLinearAllocator( void );
	SIRLinearAllocator( const SIRLinearAllocator & ) = delete;
	SIRLinearAllocator &operator=( const SIRLinearAllocator & ) = delete;

	bool init( SIRMemorySegment *pSegment, uint64_t nCommitIncrementBytes = 0, uint64_t nUsableBytes = 0 );

	void *alloc( uint64_t nBytes, uint64_t nAlignment = 16 );
	bool precommit( uint64_t nBytes );
	uint64_t getMark( void ) const;
	bool rewind( uint64_t nMark );
	bool reset( bool bDecommit = false );
	bool shutdown( void );
	bool trimTo( uint64_t nKeepBytes );

	uint64_t getUsedBytes( void ) const;
	uint64_t getCommittedPrefixBytes( void ) const;
	uint64_t getCapacity( void ) const;
	SIRMemorySegment *getSegment( void ) const;

private:
	SIRMemorySegment *m_pSegment;
	uint64_t m_nUsedBytes;
	uint64_t m_nCommittedPrefixBytes;
	uint64_t m_nCapacity;
	uint64_t m_nCommitIncrementBytes;
};

#endif // __SIRENGINE_SIRLINEARALLOCATOR_H__

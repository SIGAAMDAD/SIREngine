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

#ifndef __SIRENGINE_SIRHIGHLOWALLOCATOR_H__
#define __SIRENGINE_SIRHIGHLOWALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>

class SIRMemorySegment;

// Two-ended hunk: low allocations grow upward; high allocations grow downward.
// Separate marks can rewind either end independently. No individual free().
// trimUnusedPages decommits only complete pages not containing live allocations.
class SIRHighLowAllocator
{
public:
	SIRHighLowAllocator( void );
	SIRHighLowAllocator( const SIRHighLowAllocator & ) = delete;
	SIRHighLowAllocator &operator=( const SIRHighLowAllocator & ) = delete;

	bool init( SIRMemorySegment *pSegment );

	void *allocLow( uint64_t nBytes, uint64_t nAlignment = 16 );
	void *allocHigh( uint64_t nBytes, uint64_t nAlignment = 16 );

	uint64_t getLowMark( void ) const;
	uint64_t getHighMark( void ) const;
	bool rewindLow( uint64_t nMark );
	bool rewindHigh( uint64_t nMark );
	bool reset( void );
	bool trimUnusedPages( void );
	bool shutdown( void );

	uint64_t getLowUsedBytes( void ) const;
	uint64_t getHighUsedBytes( void ) const;
	uint64_t getAvailableBytes( void ) const;

private:
	SIRMemorySegment *m_pSegment;
	uint64_t m_nLowUsed;
	uint64_t m_nHighUsed;
	uint64_t m_nLowCommittedEnd;
	uint64_t m_nHighCommittedBegin;
};

#endif // __SIRENGINE_SIRHIGHLOWALLOCATOR_H__

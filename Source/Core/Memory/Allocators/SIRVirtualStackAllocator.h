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

#ifndef __SIRENGINE_SIRVIRTUALSTACKALLOCATOR_H__
#define __SIRENGINE_SIRVIRTUALSTACKALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRLinearAllocator.h>

// Retain all commitment, release all on empty, or release idle excess on empty.
typedef enum sirStackDecommitMode_e : uint8_t
{
	SIR_STACK_DECOMMIT_ON_SHUTDOWN = 0,
	SIR_STACK_DECOMMIT_ON_EMPTY,
	SIR_STACK_DECOMMIT_EXCESS_ON_EMPTY
} sirStackDecommitMode_t;

class SIRVirtualStackAllocator;

// C++17 return-value elision makes the intentionally nonmovable bookmark usable.
// Bookmarks must be destroyed in LIFO order before the allocator is destroyed.
class SIRStackBookmark
{
public:
	~SIRStackBookmark();

	SIRStackBookmark( const SIRStackBookmark& ) = delete;
	SIRStackBookmark& operator=( const SIRStackBookmark& ) = delete;
	SIRStackBookmark( SIRStackBookmark&& ) = delete;
	SIRStackBookmark& operator=( SIRStackBookmark&& ) = delete;

private:
	friend class SIRVirtualStackAllocator;
	SIRStackBookmark( SIRVirtualStackAllocator *pOwner, uint64_t nMark, uint32_t nDepth );

	SIRVirtualStackAllocator *m_pOwner;
	uint64_t m_nMark;
	uint32_t m_nDepth;
};

// UE-style virtual stack adapted to a pre-reserved SIRMemorySegment.
// Last page stays uncommitted as a guard; a local linear allocator owns the
// usable prefix. Strictly single-threaded owner, with scoped LIFO rewinds.
class SIRVirtualStackAllocator
{
public:
	SIRVirtualStackAllocator( void );
	SIRVirtualStackAllocator( const SIRVirtualStackAllocator& ) = delete;
	SIRVirtualStackAllocator& operator=( const SIRVirtualStackAllocator& ) = delete;
	SIRVirtualStackAllocator( SIRVirtualStackAllocator&& ) = delete;
	SIRVirtualStackAllocator& operator=( SIRVirtualStackAllocator&& ) = delete;

	bool init( SIRMemorySegment *pSegment, sirStackDecommitMode_t eMode, uint64_t nCommitIncrementBytes = 0 );

	void *alloc( uint64_t nBytes, uint64_t nAlignment = 16 );
	bool precommit( uint64_t nBytes );
	SIRStackBookmark createScopedBookmark( void );
	bool reset( void );
	bool shutdown( void );

	uint64_t getUsedBytes( void ) const;
	uint64_t getCommittedBytes( void ) const;
	uint64_t getCapacity( void ) const;

private:
	friend class SIRStackBookmark;

	void popBookmark( uint64_t nMark, uint32_t nDepth );
	bool onStackEmpty( void );

	SIRLinearAllocator m_Linear;
	uint64_t m_nRecentHighWaterMark;
	uint32_t m_nBookmarkDepth;
	sirStackDecommitMode_t m_eMode;
};

#endif // __SIRENGINE_SIRVIRTUALSTACKALLOCATOR_H__

/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRALLOCATORTESTCOMMON_H__
#define __SIRENGINE_SIRALLOCATORTESTCOMMON_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdint.h>
#include <string.h>
#include <vector>

// Test-only deterministic reference data. These STL allocations are NOT
// SIREngine-owned allocations and do not enter the allocator under test.
typedef struct sirTestLiveBlock_s
{
	void *pMemory;
	uint32_t nRequested;
	uint8_t nPattern;
} sirTestLiveBlock_t;

class SIRTestMemory
{
public:
	SIRTestMemory( void ) : m_nPage( SIRVirtualMemory::getPageSize() ), m_bReady( false )
	{
		if ( m_nPage == 0 ) {
			return;
		}
		const sirMemoryConfig_t config = { m_nPage * 16384, m_nPage * 8192, m_nPage * 1024, 128 };
		m_bReady = m_Manager.init( config );
	}

	bool ready( void ) const
	{
		return m_bReady;
	}
	SIRMemorySegment *segment( const char *szName, uint64_t nPages, sirMemoryDomain_t eDomain = SIR_MEMORY_DOMAIN_GAMEPLAY )
	{
		return m_Manager.createSegment( szName, m_nPage * nPages, 0, eDomain );
	}
	uint64_t page( void ) const
	{
		return m_nPage;
	}
	SIRMemoryManager& manager( void )
	{
		return m_Manager;
	}

private:
	SIRMemoryManager m_Manager;
	uint64_t m_nPage;
	bool m_bReady;
};

class SIRTestRandom
{
public:
	explicit SIRTestRandom( uint64_t nSeed ) : m_nState( nSeed == 0 ? 1 : nSeed )
	{
	}
	uint32_t next( void )
	{
		m_nState ^= m_nState << 13;
		m_nState ^= m_nState >> 7;
		m_nState ^= m_nState << 17;
		return ( uint32_t )( m_nState >> 16 );
	}

private:
	uint64_t m_nState;
};

static bool sirTestDisjoint( const std::vector<sirTestLiveBlock_t>& blocks, const void *pMemory, uint64_t nBytes )
{
	const uintptr_t nStart = ( uintptr_t )pMemory;
	if ( nBytes == 0 || nStart > UINTPTR_MAX - nBytes ) {
		return false;
	}
	for ( const sirTestLiveBlock_t& block : blocks ) {
		const uintptr_t nExisting = ( uintptr_t )block.pMemory;
		if ( nExisting < nStart + nBytes && nStart < nExisting + block.nRequested ) {
			return false;
		}
	}
	return true;
}

static bool sirTestPattern( const sirTestLiveBlock_t& block )
{
	const uint8_t *pBytes = ( const uint8_t * )block.pMemory;
	for ( uint32_t iByte = 0; iByte < block.nRequested; ++iByte ) {
		if ( pBytes[iByte] != block.nPattern ) {
			return false;
		}
	}
	return true;
}

// Every trace starts with a fixed seed and keeps a byte-for-byte ledger of
// live records, validating disjointness, requested sizes and alignment.
template <typename TAllocator>
static bool sirTestVariableTrace( TAllocator& allocator, uint32_t nIterations, uint32_t nMaximumSize, uint64_t nSeed )
{
	SIRTestRandom random( nSeed );
	std::vector<sirTestLiveBlock_t> live;
	uint32_t nSuccesses = 0;
	for ( uint32_t iOperation = 0; iOperation < nIterations; ++iOperation ) {
		const bool bFree = !live.empty() && ( live.size() >= 96 || ( random.next() % 100 ) < 43 );
		if ( bFree ) {
			const uint32_t iFree = random.next() % ( uint32_t )live.size();
			const sirTestLiveBlock_t block = live[iFree];
			if ( !sirTestPattern( block ) || !allocator.free( block.pMemory ) ) {
				return false;
			}
			live[iFree] = live.back();
			live.pop_back();
			continue;
		}
		const uint32_t nBytes = 1 + random.next() % nMaximumSize;
		const uint32_t nAlignment = 16u << ( random.next() % 4 );
		void *pNew = allocator.alloc( nBytes, nAlignment );
		if ( pNew == nullptr ) {
			continue;
		}
		if ( ( ( uintptr_t )pNew % nAlignment ) != 0 || allocator.getUsableSize( pNew ) < nBytes ||
		     !sirTestDisjoint( live, pNew, nBytes ) ) {
			return false;
		}
		const uint8_t nPattern = ( uint8_t )( 1 + random.next() % 254 );
		memset( pNew, nPattern, nBytes );
		live.push_back( { pNew, nBytes, nPattern } );
		++nSuccesses;
		if ( ( iOperation & 63 ) == 0 ) {
			for ( const sirTestLiveBlock_t& block : live ) {
				if ( !sirTestPattern( block ) ) {
					return false;
				}
			}
		}
	}
	for ( const sirTestLiveBlock_t& block : live ) {
		if ( !sirTestPattern( block ) || !allocator.free( block.pMemory ) ) {
			return false;
		}
	}
	return nSuccesses > nIterations / 5;
}

template <typename TAllocator>
static bool sirTestFixedTrace( TAllocator& allocator, uint32_t nIterations, uint32_t nPayloadBytes, uint64_t nSeed )
{
	SIRTestRandom random( nSeed );
	std::vector<sirTestLiveBlock_t> live;
	uint32_t nSuccesses = 0;
	for ( uint32_t iOperation = 0; iOperation < nIterations; ++iOperation ) {
		const bool bFree = !live.empty() && ( live.size() >= 192 || ( random.next() % 100 ) < 45 );
		if ( bFree ) {
			const uint32_t iFree = random.next() % ( uint32_t )live.size();
			const sirTestLiveBlock_t block = live[iFree];
			if ( !sirTestPattern( block ) || !allocator.free( block.pMemory ) ) {
				return false;
			}
			live[iFree] = live.back();
			live.pop_back();
			continue;
		}
		void *pNew = allocator.alloc();
		if ( pNew == nullptr ) {
			continue;
		}
		if ( ( ( uintptr_t )pNew % 16 ) != 0 || !sirTestDisjoint( live, pNew, nPayloadBytes ) ) {
			return false;
		}
		const uint8_t nPattern = ( uint8_t )( 1 + random.next() % 254 );
		memset( pNew, nPattern, nPayloadBytes );
		live.push_back( { pNew, nPayloadBytes, nPattern } );
		++nSuccesses;
	}
	for ( const sirTestLiveBlock_t& block : live ) {
		if ( !sirTestPattern( block ) || !allocator.free( block.pMemory ) ) {
			return false;
		}
	}
	return nSuccesses > nIterations / 5;
}

#endif // __SIRENGINE_SIRALLOCATORTESTCOMMON_H__

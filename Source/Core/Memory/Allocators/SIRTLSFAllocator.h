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

#ifndef __SIRENGINE_SIRTLSFALLOCATOR_H__
#define __SIRENGINE_SIRTLSFALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRHeapBlock.h>

class SIRMemorySegment;

// Two-level segregated-fit heap, 32 first-level and 16 second-level bins.
// Offset-indexed intrusive nodes and 16-byte physical boundary tags.
class SIRTLSFAllocator
{
public:
	SIRTLSFAllocator( void );
	SIRTLSFAllocator( const SIRTLSFAllocator& ) = delete;
	SIRTLSFAllocator& operator=( const SIRTLSFAllocator& ) = delete;

	bool init( SIRMemorySegment *pSegment, uint32_t nCommitIncrementBytes = 65536 );
	void *alloc( uint32_t nBytes, uint32_t nAlignment = 16 );
	bool free( void *pMemory );
	uint32_t getUsableSize( const void *pMemory ) const;
	bool trimUnusedPages( void );
	bool shutdown( void );

	uint32_t getAllocationCount( void ) const;
	uint32_t getCommittedBytes( void ) const;
	uint32_t getAllocatedBlockBytes( void ) const;

private:
	static uint32_t firstSet( uint32_t nWord );
	static uint32_t highestSet( uint32_t nWord );
	static bool mapSize( uint32_t nSize, bool bRoundUp, uint32_t *pFirstLevel, uint32_t *pSecondLevel );
	uint32_t findSuitable( uint32_t nBytes ) const;
	sirHeapBlock_t *block( uint32_t nOffset ) const;
	void insertFree( uint32_t nOffset );
	void removeFree( uint32_t nOffset );
	void updateNextPrev( uint32_t nOffset );
	bool grow( uint32_t nMinimumBytes );
	bool requiredBytes( uint32_t nBytes, uint32_t nAlignment, uint32_t *pBytes ) const;
	uint32_t usedBytes( uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment, uint32_t *pUserOffset ) const;
	void *allocateBlock( uint32_t nOffset, uint32_t nUsed, uint32_t nUserOffset );

	SIRMemorySegment *m_pSegment;
	uint8_t *m_pBase;
	uint32_t m_nCommittedBytes;
	uint32_t m_nCapacityBytes;
	uint32_t m_nIncrementBytes;
	uint32_t m_nPageSize;
	uint32_t m_nLastBlock;
	uint32_t m_nAllocatedBlockBytes;
	uint32_t m_nAllocationCount;
	uint32_t m_nFirstLevelBitmap;
	uint32_t m_nSecondLevelBitmap[ 32 ];
	uint32_t m_nFreeHeads[ 32 ][ 16 ];
};

#endif // __SIRENGINE_SIRTLSFALLOCATOR_H__

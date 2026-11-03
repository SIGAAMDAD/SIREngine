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

#ifndef __SIRENGINE_SIRMEMORYSEGMENT_H__
#define __SIRENGINE_SIRMEMORYSEGMENT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRMemoryManager;

#include <Core/Memory/SIRMemoryManager.h>

// Page provider for one exclusive allocator owner. No object-size or free-list
// knowledge. A successful decommit invalidates ALL pointers in those pages.
class SIRMemorySegment
{
public:
	SIRMemorySegment( void );
	SIRMemorySegment( const SIRMemorySegment & ) = delete;
	SIRMemorySegment &operator=( const SIRMemorySegment & ) = delete;

	void *getBase( void ) const;
	const char *getName( void ) const;
	uint64_t getPageSize( void ) const;
	uint64_t getReservedBytes( void ) const;
	uint64_t getCommittedBytes( void ) const;
	uint64_t getCommitLimitBytes( void ) const;
	sirMemoryDomain_t getDomain( void ) const;

	// Offsets and sizes must be multiples of the OS page size.
	bool commit( uint64_t nOffset, uint64_t nBytes );
	bool decommit( uint64_t nOffset, uint64_t nBytes );
	// Exclusive owner registration is one-time; the caller guarantees all
	// allocations and borrowed/pinned references are released before retire.
	bool bindRetirementOwner( uint32_t nOwnerId, uint32_t nOwnerGeneration );
	bool retireIdleRange( uint64_t nOffset, uint64_t nBytes, uint32_t nOwnerId, uint32_t nOwnerGeneration );
	bool cancelRetiredRange( uint64_t nOffset, uint64_t nBytes, uint32_t nOwnerId, uint32_t nOwnerGeneration );

	SIRMemoryManager *getManager( void ) const;

private:
	friend class SIRMemoryManager;

	SIRMemoryManager *m_pManager;
	uint8_t *m_pBase;
	uint64_t *m_pCommittedBits;
	uint64_t *m_pPendingDecommitBits;
	uint32_t m_nRetirementOwnerId;
	uint32_t m_nRetirementOwnerGeneration;
	uint64_t m_nReservedBytes;
	uint64_t m_nCommittedBytes;
	uint64_t m_nCommitLimitBytes;
	uint64_t m_nPageCount;
	sirMemoryDomain_t m_eDomain;
	char m_szName[32];
};

#endif // __SIRENGINE_SIRMEMORYSEGMENT_H__

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

#ifndef __SIRENGINE_SIRSIZECLASSALLOCATOR_H__
#define __SIRENGINE_SIRSIZECLASSALLOCATOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <cstdint>

class SIRMemorySegment;

// Concrete pool composite. The legacy eight power-of-two classes remain the
// default. initFine enables 32 denser classes using 32 exclusive segments.
// Pool controllers are stored in the manager's permanent metadata reservation.
// The caller supplies the requested byte size on free; no object headers.
class SIRSizeClassAllocator
{
public:
	static constexpr uint32_t SIR_SIZE_CLASS_COUNT = 8;
	static constexpr uint32_t SIR_FINE_SIZE_CLASS_COUNT = 32;

	SIRSizeClassAllocator( void );
	SIRSizeClassAllocator( const SIRSizeClassAllocator & ) = delete;
	SIRSizeClassAllocator &operator=( const SIRSizeClassAllocator & ) = delete;

	bool init( SIRMemorySegment *const *ppSegments );
	bool initFine( SIRMemorySegment *const *ppSegments );
	void *alloc( uint32_t nBytes );
	bool free( void *pMemory, uint32_t nBytes );
	bool precommitClass( uint32_t nClass, uint32_t nSlots );
	bool shutdown( void );
	static uint32_t getClassIndex( uint32_t nBytes );
	static uint32_t getClassBytes( uint32_t nClass );
	static uint32_t getFineClassIndex( uint32_t nBytes );
	static uint32_t getFineClassBytes( uint32_t nClass );
	uint32_t getActiveClassCount( void ) const;

private:
	bool initInternal( SIRMemorySegment *const *ppSegments, uint32_t nClassCount, bool bFineGrained );

	SIRPoolAllocator *m_pPools;
	uint32_t m_nInitialized;
	uint32_t m_nClassCount;
	bool m_bFineGrained;
};

#endif // __SIRENGINE_SIRSIZECLASSALLOCATOR_H__

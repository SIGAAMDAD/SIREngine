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

#ifndef __SIRENGINE_SIRVIRTUALMEMORY_H__
#define __SIRENGINE_SIRVIRTUALMEMORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stddef.h>
#include <stdint.h>

typedef enum sirMemoryProtection_e : uint8_t
{
	SIR_MEMORY_NO_ACCESS = 0,
	SIR_MEMORY_READ_ONLY = 1,
	SIR_MEMORY_READ_WRITE = 2,
	SIR_MEMORY_READ_EXECUTE = 3
} sirMemoryProtection_t;

// OS page allocator. Reservation ownership and byte accounting belong to the memory manager.
class SIRVirtualMemory
{
public:
	SIRVirtualMemory( void ) = delete;
	~SIRVirtualMemory() = delete;

	static size_t getPageSize( void ) noexcept;
	static size_t getAllocationGranularity( void ) noexcept;
	static void *reserve( size_t nBytes ) noexcept;
	static bool commit( void *pAddress, size_t nBytes ) noexcept;
	static bool decommit( void *pAddress, size_t nBytes ) noexcept;
	static bool release( void *pAddress, size_t nBytes ) noexcept;
	static bool protect( void *pAddress, size_t nBytes, sirMemoryProtection_t eProtection ) noexcept;
	static void *allocatePages( size_t nBytes ) noexcept;
	static bool freePages( void *pAddress, size_t nBytes ) noexcept;
};

#endif // __SIRENGINE_SIRVIRTUALMEMORY_H__

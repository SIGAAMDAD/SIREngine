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

#ifndef __SIRENGINE_SIRPLATFORMMEMORY_H__
#define __SIRENGINE_SIRPLATFORMMEMORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Optional process fields are described by nValidFields; unavailable values are zero.
typedef enum sirProcessMemoryField_e : uint32_t
{
	SIR_PROCESS_MEMORY_RESIDENT = 1U << 0,
	SIR_PROCESS_MEMORY_PEAK_RESIDENT = 1U << 1,
	SIR_PROCESS_MEMORY_PRIVATE_COMMITTED = 1U << 2,
	SIR_PROCESS_MEMORY_VIRTUAL_ADDRESS = 1U << 3,
	SIR_PROCESS_MEMORY_PAGE_FAULTS = 1U << 4,
	SIR_PROCESS_MEMORY_ANONYMOUS_RESIDENT = 1U << 5
} sirProcessMemoryField_t;

typedef enum sirSystemMemoryField_e : uint32_t
{
	SIR_SYSTEM_MEMORY_COMMIT = 1U << 0,
	SIR_SYSTEM_MEMORY_GROUP_LIMIT = 1U << 1,
	SIR_SYSTEM_MEMORY_GROUP_USAGE = 1U << 2,
	SIR_SYSTEM_MEMORY_SWAP = 1U << 3
} sirSystemMemoryField_t;

typedef struct sirSystemMemoryStats_s
{
	uint64_t nTotalPhysicalBytes;
	uint64_t nAvailablePhysicalBytes;
	uint64_t nTotalSwapBytes;
	uint64_t nAvailableSwapBytes;
	uint64_t nCommitLimitBytes;
	uint64_t nCommittedBytes;
	uint64_t nGroupMemoryLimitBytes;
	uint64_t nGroupMemoryUsageBytes;
	uint32_t nValidFields;
} sirSystemMemoryStats_t;

typedef struct sirProcessMemoryStats_s
{
	uint64_t nResidentBytes;
	uint64_t nPeakResidentBytes;
	uint64_t nPrivateCommittedBytes;
	uint64_t nVirtualAddressBytes;
	uint64_t nPageFaultCount;
	uint64_t nAnonymousResidentBytes;
	uint32_t nValidFields;
} sirProcessMemoryStats_t;

// OS snapshots are observations, not the SIR allocation manager's accounting.
// Group limits/usage describe a cgroup or job; other processes may share them.
class SIRPlatformMemory
{
public:
	SIRPlatformMemory( void ) = delete;
	~SIRPlatformMemory() = delete;

	static bool querySystem( sirSystemMemoryStats_t *pStats ) noexcept;
	static bool queryProcess( sirProcessMemoryStats_t *pStats ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMMEMORY_H__

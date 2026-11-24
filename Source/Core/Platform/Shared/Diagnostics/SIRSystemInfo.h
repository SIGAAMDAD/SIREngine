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

#ifndef __SIR_SYSTEMINFO_H__
#define __SIR_SYSTEMINFO_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

enum class sirOperatingSystem : uint8_t {
	SIR_OS_UNKNOWN = 0,
	SIR_OS_WINDOWS = 1,
	SIR_OS_LINUX = 2
};

enum class sirCPUArchitecture : uint8_t {
	SIR_ARCH_UNKNOWN = 0,
	SIR_ARCH_X64 = 1,
	SIR_ARCH_ARM64 = 2
};

typedef struct sirSystemInfo_s
{
	char szOSName[32];
	char szOSRelease[128];
	char szHostName[256];
	uint64_t nUptimeSeconds;
	uint64_t nProcessId;
	uint32_t nOSMajor;
	uint32_t nOSMinor;
	uint32_t nOSBuild;
	uint32_t nPageSize;
	uint32_t nAllocationGranularity;
	sirOperatingSystem eOS;
	sirCPUArchitecture eArchitecture;
} sirSystemInfo_t;

// UTF-8 strings and a best-effort OS version snapshot. nOSMajor/Minor/Build
// are Windows-specific; Linux reports its kernel release as szOSRelease.
class SIRSystemInfo
{
public:
	explicit SIRSystemInfo( void ) = delete;
	explicit SIRSystemInfo( const SIRSystemInfo& ) = delete;
	explicit SIRSystemInfo( SIRSystemInfo&& ) = delete;
	~SIRSystemInfo() = delete;

	SIRSystemInfo& operator=( const SIRSystemInfo& ) = delete;
	SIRSystemInfo& operator=( SIRSystemInfo&& ) = delete;

	static bool query( sirSystemInfo_t *pInfo ) noexcept;
};

#endif // __SIR_SYSTEMINFO_H__

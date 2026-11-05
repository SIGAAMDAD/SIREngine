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

#ifndef __SIRENGINE_SIRCPUINFO_H__
#define __SIRENGINE_SIRCPUINFO_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

typedef enum sirCPUFeature_e : uint64_t
{
	SIR_CPU_FEATURE_SSE2 = 1ULL << 0,
	SIR_CPU_FEATURE_SSE41 = 1ULL << 1,
	SIR_CPU_FEATURE_SSE42 = 1ULL << 2,
	SIR_CPU_FEATURE_POPCNT = 1ULL << 3,
	SIR_CPU_FEATURE_AES = 1ULL << 4,
	SIR_CPU_FEATURE_AVX = 1ULL << 5,
	SIR_CPU_FEATURE_AVX2 = 1ULL << 6,
	SIR_CPU_FEATURE_FMA = 1ULL << 7,
	SIR_CPU_FEATURE_BMI1 = 1ULL << 8,
	SIR_CPU_FEATURE_BMI2 = 1ULL << 9,
	SIR_CPU_FEATURE_AVX512F = 1ULL << 10,
	SIR_CPU_FEATURE_NEON = 1ULL << 11
} sirCPUFeature_t;

typedef struct sirCPUInfo_s
{
	char szVendor[16];
	char szBrand[64];
	uint64_t nFeatures;
	uint64_t nL1DataCacheBytes;
	uint64_t nL2CacheBytes;
	uint64_t nL3CacheBytes;
	uint32_t nLogicalProcessorCount;
	uint32_t nPhysicalCoreCount; // zero if unavailable; never inferred from SMT ratio
	uint32_t nCacheLineBytes;
} sirCPUInfo_t;

class SIRCPUInfo
{
public:
	SIRCPUInfo( void ) = delete;
	~SIRCPUInfo() = delete;

	static bool query( sirCPUInfo_t *pInfo ) noexcept;

private:
	static void queryInstructionSet( sirCPUInfo_t *pInfo ) noexcept;
	static bool queryTopology( sirCPUInfo_t *pInfo ) noexcept;
};

#endif // __SIRENGINE_SIRCPUINFO_H__

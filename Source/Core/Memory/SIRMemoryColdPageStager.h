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

#ifndef __SIRENGINE_SIRMEMORYCOLDPAGESTAGER_H__
#define __SIRENGINE_SIRMEMORYCOLDPAGESTAGER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

class SIRMemoryManager;
class SIRMemorySegment;

typedef struct sirColdStageReport_s
{
	uint64_t nRequestedBytes;
	uint64_t nNewCommittedBytes;
	uint32_t nCompletedRanges;
	uint32_t nFailedRange;
} sirColdStageReport_t;

// Explicit cold staging only. Never decommits live data or reclaims pages.
// Entries must be submitted ordered by segment, then increasing offset.
// Adjacent/overlapping entries within one segment are grouped into one commit.
typedef struct sirStageRange_s
{
	SIRMemorySegment *pSegment;
	uint64_t nOffset;
	uint64_t nBytes;
} sirStageRange_t;

class SIRMemoryColdPageStager
{
public:
	SIRMemoryColdPageStager( void );
	bool init( SIRMemoryManager *pManager, uint32_t nCapacity );
	bool add( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes );
	sirColdStageReport_t commitStaged( void );
	void clear( void );
private:

	sirStageRange_t *m_pRanges;
	uint32_t m_nCapacity;
	uint32_t m_nCount;
	SIRMemoryManager *m_pManager;
};

#endif // __SIRENGINE_SIRMEMORYCOLDPAGESTAGER_H__

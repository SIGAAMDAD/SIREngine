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

#ifndef __SIRENGINE_SIRFMODMEMORYBUDGET_H__
#define __SIRENGINE_SIRFMODMEMORYBUDGET_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Audio/SIRAudioTypes.h>
#include <Core/Containers/SIRContainerResult.h>
#include <Core/Platform/SIRAtomic.h>

// Memory charged to the audio domain includes alignment padding and headers.
// The caller's NMD allocator remains the actual source of resident memory.
typedef struct sirFMODMemoryBudgetStats_s
{
	uint64_t nBudgetBytes;
	uint64_t nCurrentBytes;
	uint64_t nPeakBytes;
	uint64_t nAllocations;
	uint64_t nFailedAllocations;
} sirFMODMemoryBudgetStats_t;

class SIRFMODMemoryBudget
{
private:
	typedef struct sirFMODMemoryHeader_s
	{
		void *pBase;
		size_t nCharge;
	} sirFMODMemoryHeader_t;

	const sirContainerAllocator_t *m_pUpstream = nullptr;
	sirContainerAllocator_t m_Allocator = {};
	SIRAtomic<uint64_t> m_nCurrent = SIRAtomic<uint64_t>( 0 );
	SIRAtomic<uint64_t> m_nPeak = SIRAtomic<uint64_t>( 0 );
	SIRAtomic<uint64_t> m_nAllocations = SIRAtomic<uint64_t>( 0 );
	SIRAtomic<uint64_t> m_nFailures = SIRAtomic<uint64_t>( 0 );
	uint64_t m_nBudgetBytes = 0;

	static void *allocateCallback( void *pContext, size_t nBytes, size_t nAlignment ) noexcept;
	static void deallocateCallback( void *pContext, void *pMemory, size_t nBytes, size_t nAlignment ) noexcept;

public:
	SIRFMODMemoryBudget( void ) noexcept = default;
	~SIRFMODMemoryBudget( void ) noexcept;
	SIRFMODMemoryBudget( const SIRFMODMemoryBudget & ) = delete;
	SIRFMODMemoryBudget &operator=( const SIRFMODMemoryBudget & ) = delete;

	sirAudioResult_t configure( const sirContainerAllocator_t *pUpstream, uint64_t nBudgetBytes ) noexcept;
	sirAudioResult_t shutdown( void ) noexcept;
	const sirContainerAllocator_t *getAllocator( void ) const noexcept;
	sirFMODMemoryBudgetStats_t getStats( void ) const noexcept;
};
#endif // __SIRENGINE_SIRFMODMEMORYBUDGET_H__

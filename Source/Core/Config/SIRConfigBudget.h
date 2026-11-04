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

#ifndef __SIRENGINE_SIRCONFIGBUDGET_H__
#define __SIRENGINE_SIRCONFIGBUDGET_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Config/SIRConfigTypes.h>

// One budget belongs to one parser/document owner. No implicit fallback allocator.
class SIRConfigBudget
{
public:
	SIRConfigBudget( void ) noexcept;
	SIRConfigBudget( const SIRConfigBudget & ) = delete;
	SIRConfigBudget &operator=( const SIRConfigBudget & ) = delete;
	~SIRConfigBudget( void ) noexcept;

	bool init( const sirContainerAllocator_t *pBacking, uint64_t nHardLimitBytes ) noexcept;
	bool shutdown( void ) noexcept;
	const sirContainerAllocator_t *getAllocator( void ) const noexcept;
	uint64_t getLiveBytes( void ) const noexcept;
	uint64_t getPeakBytes( void ) const noexcept;
	uint64_t getAllocations( void ) const noexcept;
	uint64_t getRejectedAllocations( void ) const noexcept;

private:
	static void *allocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept;
	static void deallocate( void *pContext, void *pMemory, size_t nBytes, size_t nAlignment ) noexcept;

	const sirContainerAllocator_t *m_pBacking;
	sirContainerAllocator_t m_Allocator;
	uint64_t m_nLimit;
	uint64_t m_nLive;
	uint64_t m_nPeak;
	uint64_t m_nAllocations;
	uint64_t m_nRejected;
};

#endif // __SIRENGINE_SIRCONFIGBUDGET_H__

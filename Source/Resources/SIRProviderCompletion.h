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

#ifndef __SIRENGINE_SIRPROVIDERCOMPLETION_H__
#define __SIRENGINE_SIRPROVIDERCOMPLETION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Stable provider-local lanes, independent of GPU queue names or native
// devices.
constexpr uint32_t SIR_PROVIDER_MAX_LANES = 4;

// A required retirement point or an observed completion snapshot. Values from
// different lanes or provider generations must never be compared as scalars.
typedef struct sirProviderCompletionSet_s
{
	uint32_t nProviderId;
	uint32_t nProviderGeneration;
	uint32_t nLaneMask;
	uint32_t nLaneCount;
	uint64_t szCompletedValues[SIR_PROVIDER_MAX_LANES];
} sirProviderCompletionSet_t;

static_assert( sizeof( sirProviderCompletionSet_t ) == 48, "provider completion ABI layout changed" );

static inline bool sirProviderCompletionValid( const sirProviderCompletionSet_t& rCompletion ) noexcept
{
	if ( rCompletion.nProviderId == 0 || rCompletion.nProviderGeneration == 0 || rCompletion.nLaneCount == 0 ||
	     rCompletion.nLaneCount > SIR_PROVIDER_MAX_LANES || rCompletion.nLaneMask == 0 ) {
		return false;
	}
	const uint32_t nAllowedMask = ( 1u << rCompletion.nLaneCount ) - 1u;
	if ( ( rCompletion.nLaneMask & ~nAllowedMask ) != 0 ) {
		return false;
	}
	for ( uint32_t iLane = 0; iLane < SIR_PROVIDER_MAX_LANES; ++iLane ) {
		if ( ( rCompletion.nLaneMask & ( 1u << iLane ) ) == 0 && rCompletion.szCompletedValues[iLane] != 0 ) {
			return false;
		}
	}
	return true;
}

static inline bool sirProviderCompletionSatisfied( const sirProviderCompletionSet_t& rRequired,
                                                   const sirProviderCompletionSet_t& rObserved ) noexcept
{
	if ( !sirProviderCompletionValid( rRequired ) || !sirProviderCompletionValid( rObserved ) ||
	     rRequired.nProviderId != rObserved.nProviderId || rRequired.nProviderGeneration != rObserved.nProviderGeneration ||
	     rRequired.nLaneCount != rObserved.nLaneCount || ( rRequired.nLaneMask & rObserved.nLaneMask ) != rRequired.nLaneMask ) {
		return false;
	}
	for ( uint32_t iLane = 0; iLane < rRequired.nLaneCount; ++iLane ) {
		if ( ( rRequired.nLaneMask & ( 1u << iLane ) ) != 0 && rObserved.szCompletedValues[iLane] < rRequired.szCompletedValues[iLane] ) {
			return false;
		}
	}
	return true;
}

#endif // __SIRENGINE_SIRPROVIDERCOMPLETION_H__

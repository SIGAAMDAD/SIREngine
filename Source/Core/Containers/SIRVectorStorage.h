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

#ifndef __SIRENGINE_SIRVECTORSTORAGE_H__
#define __SIRENGINE_SIRVECTORSTORAGE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <stdint.h>

typedef enum sirContainerGrowthPolicy_e : uint8_t
{
	SIR_CONTAINER_GROWTH_ONE_AND_HALF = 0,
	SIR_CONTAINER_GROWTH_DOUBLE = 1,
	SIR_CONTAINER_GROWTH_EXACT = 2
} sirContainerGrowthPolicy_t;

class SIRVectorStorage
{
public:
	static sirContainerResult_t calculateGrowthForPolicy( uint32_t nCurrent, uint32_t nRequired, sirContainerGrowthPolicy_t ePolicy,
	                                                      uint32_t *pNewCapacity ) noexcept
	{
		if ( pNewCapacity == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nGrowth = nCurrent;
		switch ( ePolicy ) {
		case SIR_CONTAINER_GROWTH_ONE_AND_HALF:
			nGrowth = nCurrent < 8 ? 8 : ( uint64_t )nCurrent + ( ( uint64_t )nCurrent >> 1 );
			break;
		case SIR_CONTAINER_GROWTH_DOUBLE:
			nGrowth = nCurrent < 8 ? 8 : ( uint64_t )nCurrent * 2;
			break;
		case SIR_CONTAINER_GROWTH_EXACT:
			nGrowth = nRequired;
			break;
		default:
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nGrowth < nRequired ) {
			nGrowth = nRequired;
		}
		if ( nGrowth > UINT32_MAX ) {
			nGrowth = UINT32_MAX;
		}
		*pNewCapacity = ( uint32_t )nGrowth;
		return SIR_CONTAINER_OK;
	}

	static sirContainerResult_t calculateGrowth( uint32_t nCurrent, uint32_t nRequired, uint32_t *pNewCapacity ) noexcept
	{
		return calculateGrowthForPolicy( nCurrent, nRequired, SIR_CONTAINER_GROWTH_ONE_AND_HALF, pNewCapacity );
	}
};

#endif // __SIRENGINE_SIRVECTORSTORAGE_H__

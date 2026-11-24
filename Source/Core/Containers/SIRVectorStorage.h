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

#ifndef __SIR_VECTORSTORAGE_H__
#define __SIR_VECTORSTORAGE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <stdint.h>

enum class SIRContainerGrowthPolicy : uint8_t {
	SIR_CONTAINER_GROWTH_ONE_AND_HALF = 0,
	SIR_CONTAINER_GROWTH_DOUBLE = 1,
	SIR_CONTAINER_GROWTH_EXACT = 2
};

class SIRVectorStorage
{
public:
	static SIRContainerResult calculateGrowthForPolicy( uint32_t nCurrent, uint32_t nRequired, SIRContainerGrowthPolicy ePolicy,
	                                                      uint32_t *pNewCapacity ) noexcept
	{
		if ( pNewCapacity == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nGrowth = nCurrent;
		switch ( ePolicy ) {
		case SIRContainerGrowthPolicy::SIR_CONTAINER_GROWTH_ONE_AND_HALF:
			nGrowth = nCurrent < 8 ? 8 : static_cast<uint64_t>(nCurrent) + ( static_cast<uint64_t>(nCurrent) >> 1 );
			break;
		case SIRContainerGrowthPolicy::SIR_CONTAINER_GROWTH_DOUBLE:
			nGrowth = nCurrent < 8 ? 8 : static_cast<uint64_t>(nCurrent) * 2;
			break;
		case SIRContainerGrowthPolicy::SIR_CONTAINER_GROWTH_EXACT:
			nGrowth = nRequired;
			break;
		default:
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nGrowth < nRequired ) {
			nGrowth = nRequired;
		}
		if ( nGrowth > UINT32_MAX ) {
			nGrowth = UINT32_MAX;
		}
		*pNewCapacity = static_cast<uint32_t>(nGrowth);
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	static SIRContainerResult calculateGrowth( uint32_t nCurrent, uint32_t nRequired, uint32_t *pNewCapacity ) noexcept
	{
		return calculateGrowthForPolicy( nCurrent, nRequired, SIRContainerGrowthPolicy::SIR_CONTAINER_GROWTH_ONE_AND_HALF, pNewCapacity );
	}
};

#endif // __SIR_VECTORSTORAGE_H__

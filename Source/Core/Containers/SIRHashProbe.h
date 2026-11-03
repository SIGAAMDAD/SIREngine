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

#ifndef __SIRENGINE_SIRHASHPROBE_H__
#define __SIRENGINE_SIRHASHPROBE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRawStorage.h>
#include <stdint.h>

// Shared probing/capacity policy for flat associative tables.
class SIRHashProbe
{
public:
	static constexpr uint8_t EMPTY = 0;
	static constexpr uint8_t OCCUPIED = 1;
	static constexpr uint8_t DELETED = 2;
	static constexpr uint32_t MIN_CAPACITY = 8;

	static sirContainerResult_t capacityForCount( uint32_t nElements, uint32_t *pCapacity ) noexcept
	{
		if ( pCapacity == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nRequired = ( ( uint64_t )nElements * 8 + 6 ) / 7;
		uint64_t nCapacity = MIN_CAPACITY;
		while ( nCapacity < nRequired ) {
			nCapacity <<= 1;
			if ( nCapacity > ( uint64_t )UINT32_MAX ) {
				return SIR_CONTAINER_SIZE_OVERFLOW;
			}
		}
		*pCapacity = ( uint32_t )nCapacity;
		return SIR_CONTAINER_OK;
	}

	static size_t mixHash( size_t nHash ) noexcept
	{
#if SIZE_MAX > UINT32_MAX
		uint64_t nMixed = ( uint64_t )nHash;
		nMixed ^= nMixed >> 30;
		nMixed *= UINT64_C( 0xbf58476d1ce4e5b9 );
		nMixed ^= nMixed >> 27;
		nMixed *= UINT64_C( 0x94d049bb133111eb );
		nMixed ^= nMixed >> 31;
		return ( size_t )nMixed;
#else
		uint32_t nMixed = ( uint32_t )nHash;
		nMixed ^= nMixed >> 16;
		nMixed *= UINT32_C( 0x85ebca6b );
		nMixed ^= nMixed >> 13;
		nMixed *= UINT32_C( 0xc2b2ae35 );
		nMixed ^= nMixed >> 16;
		return ( size_t )nMixed;
#endif
	}

	static uint32_t firstIndex( size_t nHash, uint32_t nCapacity ) noexcept
	{
		return ( uint32_t )( mixHash( nHash ) & ( nCapacity - 1 ) );
	}
};

#endif // __SIRENGINE_SIRHASHPROBE_H__

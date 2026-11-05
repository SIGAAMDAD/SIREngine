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

	// Each bucket uses two bits: empty=0, live=1, tombstone=2.
	// A table has at least eight buckets, so capacity is divisible by four.
	template <bool bCompactControl>
	static uint32_t controlBytes( uint32_t nCapacity ) noexcept
	{
		if constexpr ( bCompactControl ) {
			return nCapacity / 4;
		}
		return nCapacity;
	}

	template <bool bCompactControl>
	static uint8_t getControl( const uint8_t *pControl, uint32_t iBucket ) noexcept
	{
		if constexpr ( bCompactControl ) {
			return ( uint8_t )( ( pControl[iBucket >> 2] >> ( ( iBucket & 3 ) * 2 ) ) & 3u );
		}
		return pControl[iBucket];
	}

	template <bool bCompactControl>
	static void setControl( uint8_t *pControl, uint32_t iBucket, uint8_t nState ) noexcept
	{
		if constexpr ( bCompactControl ) {
			uint8_t& nByte = pControl[iBucket >> 2];
			uint32_t nShift = ( iBucket & 3 ) * 2;
			nByte = ( uint8_t )( ( nByte & ~( 3u << nShift ) ) | ( ( uint32_t )nState << nShift ) );
		} else {
			pControl[iBucket] = nState;
		}
	}

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
		nMixed ^= nMixed >> 33;
		nMixed *= UINT64_C( 0xff51afd7ed558ccd );
		nMixed ^= nMixed >> 33;
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

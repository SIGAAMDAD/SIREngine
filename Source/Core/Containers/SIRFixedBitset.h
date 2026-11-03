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

#ifndef __SIRENGINE_SIRFIXEDBITSET_H__
#define __SIRENGINE_SIRFIXEDBITSET_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRBitWords.h>
#include <stdint.h>

template <uint32_t N>
class SIRFixedBitset
{
private:
	static_assert( N > 0, "Fixed bitset requires nonzero bit capacity" );
	uint64_t m_szWords[( ( uint64_t )N + 63 ) / 64] = {};

public:
	constexpr uint32_t getSize( void ) const noexcept
	{
		return N;
	}
	bool test( uint32_t iBit ) const noexcept
	{
		return iBit < N && ( ( m_szWords[iBit / 64] >> ( iBit % 64 ) ) & 1u ) != 0;
	}

	sirContainerResult_t trySet( uint32_t iBit, bool bValue ) noexcept
	{
		if ( iBit >= N ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nMask = 1ull << ( iBit % 64 );
		if ( bValue ) {
			m_szWords[iBit / 64] |= nMask;
		} else {
			m_szWords[iBit / 64] &= ~nMask;
		}
		return SIR_CONTAINER_OK;
	}

	uint32_t count( void ) const noexcept
	{
		uint32_t nCount = 0;
		for ( uint32_t iWord = 0; iWord < ( ( ( uint64_t )N + 63 ) / 64 ); ++iWord ) {
			nCount += SIRBitWords::count( m_szWords[iWord] );
		}
		return nCount;
	}

	uint32_t findFirstSet( void ) const noexcept
	{
		for ( uint32_t iWord = 0; iWord < ( ( ( uint64_t )N + 63 ) / 64 ); ++iWord ) {
			if ( m_szWords[iWord] != 0 ) {
				return iWord * 64 + SIRBitWords::firstBit( m_szWords[iWord] );
			}
		}
		return UINT32_MAX;
	}

	void clear( void ) noexcept
	{
		memset( m_szWords, 0, sizeof( m_szWords ) );
	}
};

#endif // __SIRENGINE_SIRFIXEDBITSET_H__

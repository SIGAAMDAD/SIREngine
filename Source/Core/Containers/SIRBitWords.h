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

#ifndef __SIRENGINE_SIRBITWORDS_H__
#define __SIRENGINE_SIRBITWORDS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <stdint.h>

class SIRBitWords
{
public:
	static constexpr uint32_t BITS_PER_WORD = 64;

	static uint64_t maskTail( uint32_t nValidBits ) noexcept
	{
		if ( nValidBits == 0 || nValidBits >= BITS_PER_WORD ) {
			return UINT64_MAX;
		}
		return ( 1ull << nValidBits ) - 1;
	}

	static uint32_t count( uint64_t nWord ) noexcept
	{
		nWord -= ( nWord >> 1 ) & 0x5555555555555555ull;
		nWord = ( nWord & 0x3333333333333333ull ) + ( ( nWord >> 2 ) & 0x3333333333333333ull );
		nWord = ( nWord + ( nWord >> 4 ) ) & 0x0f0f0f0f0f0f0f0full;
		return ( uint32_t )( ( nWord * 0x0101010101010101ull ) >> 56 );
	}

	static uint32_t firstBit( uint64_t nWord ) noexcept
	{
		if ( nWord == 0 ) {
			return 64;
		}
		uint32_t iBit = 0;
		while ( ( nWord & 1 ) == 0 ) {
			nWord >>= 1;
			++iBit;
		}
		return iBit;
	}
};

#endif // __SIRENGINE_SIRBITWORDS_H__

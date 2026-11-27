/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_FIXEDBITSET_H__
#define __SIR_FIXEDBITSET_H__

#include <Core/SIRPCH.h>

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
public:
	using size_type = uint32_t;

	constexpr uint32_t size( void ) const noexcept
	{
		return getSize();
	}

	constexpr bool empty( void ) const noexcept
	{
		return getSize() == 0;
	}

	constexpr bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	constexpr uint32_t getSize( void ) const noexcept
	{
		return N;
	}

	bool test( uint32_t iBit ) const noexcept
	{
		return iBit < N && ( ( m_szWords[iBit / 64] >> ( iBit % 64 ) ) & 1u ) != 0;
	}

	SIRContainerResult trySet( uint32_t iBit, bool bValue ) noexcept
	{
		if ( iBit >= N ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nMask = 1ull << ( iBit % 64 );
		if ( bValue ) {
			m_szWords[iBit / 64] |= nMask;
		} else {
			m_szWords[iBit / 64] &= ~nMask;
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	using value_type = bool;

	bool any( void ) const noexcept
	{
		return findFirstSet() != UINT32_MAX;
	}
	bool none( void ) const noexcept
	{
		return !any();
	}
	bool all( void ) const noexcept
	{
		return count() == size();
	}
	bool contains( uint32_t iBit ) const noexcept
	{
		return test( iBit );
	}

	SIRContainerResult tryReset( uint32_t iBit ) noexcept
	{
		return trySet( iBit, false );
	}
	SIRContainerResult tryFlip( uint32_t iBit ) noexcept
	{
		if ( iBit >= size() )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		return trySet( iBit, !test( iBit ) );
	}

	void setAll( void ) noexcept
	{
		for ( uint32_t iWord = 0; iWord < ( static_cast<uint64_t>( N ) + 63 ) / 64; ++iWord ) {
			m_szWords[iWord] = UINT64_MAX;
		}
		m_szWords[( N - 1 ) / 64] &= SIRBitWords::maskTail( N % 64 );
	}

	void flipAll( void ) noexcept
	{
		for ( uint32_t iWord = 0; iWord < ( static_cast<uint64_t>( N ) + 63 ) / 64; ++iWord ) {
			m_szWords[iWord] = ~m_szWords[iWord];
		}
		m_szWords[( N - 1 ) / 64] &= SIRBitWords::maskTail( N % 64 );
	}

	void reset( void ) noexcept
	{
		clear();
	}

	uint32_t count( void ) const noexcept
	{
		uint32_t nCount = 0;
		for ( uint32_t iWord = 0; iWord < ( ( static_cast<uint64_t>( N ) + 63 ) / 64 ); ++iWord ) {
			nCount += SIRBitWords::count( m_szWords[iWord] );
		}
		return nCount;
	}

	uint32_t findFirstSet( void ) const noexcept
	{
		for ( uint32_t iWord = 0; iWord < ( ( static_cast<uint64_t>( N ) + 63 ) / 64 ); ++iWord ) {
			if ( m_szWords[iWord] != 0 ) {
				return iWord * 64 + SIRBitWords::firstBit( m_szWords[iWord] );
			}
		}
		return UINT32_MAX;
	}

	// Enumerate only populated bit positions, in ascending order. The callback
	// may read this bitset but must not structurally modify it during traversal.
	template <typename Visitor>
	void forEachSetBit( Visitor&& visitor ) const
	{
		for ( uint32_t iWord = 0; iWord < ( ( static_cast<uint64_t>( N ) + 63 ) / 64 ); ++iWord ) {
			uint64_t nBits = m_szWords[iWord];
			while ( nBits != 0 ) {
				visitor( iWord * 64 + SIRBitWords::firstBit( nBits ) );
				nBits &= nBits - 1;
			}
		}
	}

	void clear( void ) noexcept
	{
		memset( m_szWords, 0, sizeof( m_szWords ) );
	}

private:
	static_assert( N > 0, "Fixed bitset requires nonzero bit capacity" );
	uint64_t m_szWords[( static_cast<uint64_t>( N ) + 63 ) / 64] = {};
};

#endif // __SIR_FIXEDBITSET_H__

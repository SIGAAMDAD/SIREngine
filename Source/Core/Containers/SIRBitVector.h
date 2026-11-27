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

#ifndef __SIR_BITVECTOR_H__
#define __SIR_BITVECTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRBitWords.h>
#include <Core/Containers/SIRVector.h>

class SIRBitVector
{
public:
	using size_type = uint32_t;

	uint32_t size( void ) const noexcept
	{
		return getSize();
	}

	bool empty( void ) const noexcept
	{
		return getSize() == 0;
	}

	bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	explicit SIRBitVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_words( pAllocator )
	{
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nBits;
	}

	uint32_t getWordCount( void ) const noexcept
	{
		return m_words.getSize();
	}

	SIRContainerResult tryResize( uint32_t nBits )
	{
		uint32_t nWords = ( uint32_t )( ( static_cast<uint64_t>( nBits ) + 63 ) / 64 );
		SIRContainerResult eResult = m_words.tryResize( nWords );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_nBits = nBits;
		if ( nWords != 0 ) {
			m_words[nWords - 1] &= SIRBitWords::maskTail( nBits % 64 );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool test( uint32_t iBit ) const noexcept
	{
		return iBit < m_nBits && ( ( m_words[iBit / 64] >> ( iBit % 64 ) ) & 1u ) != 0;
	}

	SIRContainerResult trySet( uint32_t iBit, bool bValue ) noexcept
	{
		if ( iBit >= m_nBits ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nMask = 1ull << ( iBit % 64 );
		if ( bValue ) {
			m_words[iBit / 64] |= nMask;
		} else {
			m_words[iBit / 64] &= ~nMask;
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
		for ( uint32_t iWord = 0; iWord < m_words.size(); ++iWord ) {
			m_words[iWord] = UINT64_MAX;
		}
		if ( !m_words.empty() )
		{
			m_words.back() &= SIRBitWords::maskTail( m_nBits % 64 );
		}
	}

	void flipAll( void ) noexcept
	{
		for ( uint32_t iWord = 0; iWord < m_words.size(); ++iWord ) {
			m_words[iWord] = ~m_words[iWord];
		}
		if ( !m_words.empty() )
		{
			m_words.back() &= SIRBitWords::maskTail( m_nBits % 64 );
		}
	}

	void reset( void ) noexcept
	{
		clear();
	}

	uint32_t count( void ) const noexcept
	{
		uint32_t nTotal = 0;
		for ( uint32_t iWord = 0; iWord < m_words.getSize(); ++iWord ) {
			nTotal += SIRBitWords::count( m_words[iWord] );
		}
		return nTotal;
	}

	uint32_t findFirstSet( void ) const noexcept
	{
		for ( uint32_t iWord = 0; iWord < m_words.getSize(); ++iWord ) {
			uint64_t nWord = m_words[iWord];
			if ( nWord != 0 ) {
				return iWord * 64 + SIRBitWords::firstBit( nWord );
			}
		}
		return UINT32_MAX;
	}

	// Enumerate only populated bit positions, in ascending order. The callback
	// may read this bitset but must not structurally modify it during traversal.
	template <typename Visitor>
	void forEachSetBit( Visitor&& visitor ) const
	{
		for ( uint32_t iWord = 0; iWord < m_words.getSize(); ++iWord ) {
			uint64_t nBits = m_words[iWord];
			while ( nBits != 0 ) {
				visitor( iWord * 64 + SIRBitWords::firstBit( nBits ) );
				nBits &= nBits - 1;
			}
		}
	}

	SIRContainerResult tryShrinkToFit( void )
	{
		return m_words.tryShrinkToFit();
	}

	void clearAndRelease( void ) noexcept
	{
		m_words.clear();
		m_nBits = 0;
		m_words.tryShrinkToFit();
	}

	void clear( void ) noexcept
	{
		for ( uint32_t iWord = 0; iWord < m_words.getSize(); ++iWord ) {
			m_words[iWord] = 0;
		}
	}

private:
	SIRVector<uint64_t> m_words;
	uint32_t m_nBits = 0;
};

#endif // __SIR_BITVECTOR_H__

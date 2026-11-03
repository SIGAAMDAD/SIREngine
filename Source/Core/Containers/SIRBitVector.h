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

#ifndef __SIRENGINE_SIRBITVECTOR_H__
#define __SIRENGINE_SIRBITVECTOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRBitWords.h>
#include <Core/Containers/SIRVector.h>

class SIRBitVector
{
private:
	SIRVector<uint64_t> m_words;
	uint32_t m_nBits = 0;

public:
	explicit SIRBitVector( const sirContainerAllocator_t *pAllocator ) noexcept : m_words( pAllocator )
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

	sirContainerResult_t tryResize( uint32_t nBits )
	{
		uint32_t nWords = ( uint32_t )( ( ( uint64_t )nBits + 63 ) / 64 );
		if ( nWords > m_words.getSize() ) {
			sirContainerResult_t eResult = m_words.tryReserve( nWords );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			while ( m_words.getSize() < nWords ) {
				m_words.tryEmplaceBack( 0ull );
			}
		} else {
			while ( m_words.getSize() > nWords ) {
				m_words.tryPopBack();
			}
		}
		m_nBits = nBits;
		if ( nWords != 0 ) {
			m_words[nWords - 1] &= SIRBitWords::maskTail( nBits % 64 );
		}
		return SIR_CONTAINER_OK;
	}

	bool test( uint32_t iBit ) const noexcept
	{
		return iBit < m_nBits && ( ( m_words[iBit / 64] >> ( iBit % 64 ) ) & 1u ) != 0;
	}

	sirContainerResult_t trySet( uint32_t iBit, bool bValue ) noexcept
	{
		if ( iBit >= m_nBits ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint64_t nMask = 1ull << ( iBit % 64 );
		if ( bValue ) {
			m_words[iBit / 64] |= nMask;
		} else {
			m_words[iBit / 64] &= ~nMask;
		}
		return SIR_CONTAINER_OK;
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

	void clear( void ) noexcept
	{
		for ( uint32_t iWord = 0; iWord < m_words.getSize(); ++iWord ) {
			m_words[iWord] = 0;
		}
	}
};

#endif // __SIRENGINE_SIRBITVECTOR_H__

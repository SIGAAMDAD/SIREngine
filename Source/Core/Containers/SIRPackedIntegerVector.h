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

/* SIREngine MPLv2 - sealed fixed-bit-width integer vector. */
#ifndef __SIR_PACKEDINTEGERVECTOR_H__
#define __SIR_PACKEDINTEGERVECTOR_H__
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRVector.h>
#include <stdint.h>
#include <string.h>

// Format v1 is byte-order independent, word-aligned, and self-delimiting.
// The first 4 bytes are ASCII S I R P; all integer fields are little-endian.
class SIRPackedIntegerEncoding
{
public:
	static constexpr uint32_t MAGIC = 0x50524953;
	static constexpr uint32_t HEADER_BYTES = 16;

	static uint32_t countWords( uint64_t nCount, uint8_t nBits ) noexcept
	{
		if ( nBits == 0 || nBits > 64 || nCount > UINT32_MAX ) {
			return 0;
		}
		const uint64_t nWords = ( nCount * nBits + 63 ) / 64;
		return nWords > UINT32_MAX ? 0 : static_cast<uint32_t>( nWords );
	}

	static uint32_t read32( const uint8_t *pBytes ) noexcept
	{
		return uint32_t( pBytes[0] ) | ( uint32_t( pBytes[1] ) << 8 ) | ( uint32_t( pBytes[2] ) << 16 ) | ( uint32_t( pBytes[3] ) << 24 );
	}

	static uint64_t read64( const uint8_t *pBytes ) noexcept
	{
		return static_cast<uint64_t>( read32( pBytes ) ) | ( static_cast<uint64_t>( read32( pBytes + 4 ) ) << 32 );
	}

	static void write32( uint8_t *pBytes, uint32_t nValue ) noexcept
	{
		for ( uint32_t i = 0; i < 4; ++i ) {
			pBytes[i] = static_cast<uint8_t>( nValue >> ( i * 8 ) );
		}
	}

	static void write64( uint8_t *pBytes, uint64_t nValue ) noexcept
	{
		write32( pBytes, static_cast<uint32_t>( nValue ) );
		write32( pBytes + 4, static_cast<uint32_t>( nValue >> 32 ) );
	}
};

class SIRPackedIntegerReadView
{
public:
	explicit SIRPackedIntegerReadView( void ) noexcept = default;

	static SIRContainerResult tryDecode( const uint8_t *pBytes, uint64_t nBytes, SIRPackedIntegerReadView *pView ) noexcept
	{
		if ( pBytes == nullptr || pView == nullptr || nBytes < SIRPackedIntegerEncoding::HEADER_BYTES ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( SIRPackedIntegerEncoding::read32( pBytes ) != SIRPackedIntegerEncoding::MAGIC || pBytes[4] != 1 || pBytes[5] == 0 ||
		     pBytes[5] > 64 || pBytes[6] != 0 || pBytes[7] != 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint32_t nCount = SIRPackedIntegerEncoding::read32( pBytes + 8 );
		const uint32_t nWords = SIRPackedIntegerEncoding::read32( pBytes + 12 );
		if ( nWords != SIRPackedIntegerEncoding::countWords( nCount, pBytes[5] ) ||
		     nBytes != static_cast<uint64_t>( SIRPackedIntegerEncoding::HEADER_BYTES ) + static_cast<uint64_t>( nWords ) * 8 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		pView->m_pWords = pBytes + SIRPackedIntegerEncoding::HEADER_BYTES;
		pView->m_nCount = nCount;
		pView->m_nBits = pBytes[5];
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nCount;
	}

	uint8_t getBitWidth( void ) const noexcept
	{
		return m_nBits;
	}

	bool tryGet( uint32_t iIndex, uint64_t *pValue ) const noexcept
	{
		if ( iIndex >= m_nCount || pValue == nullptr || m_pWords == nullptr ) {
			return false;
		}
		const uint64_t nBitOffset = static_cast<uint64_t>( iIndex ) * m_nBits;
		const uint64_t nWord = nBitOffset / 64;
		const uint32_t nShift = static_cast<uint32_t>( nBitOffset & 63 );
		uint64_t nValue = SIRPackedIntegerEncoding::read64( m_pWords + nWord * 8 ) >> nShift;
		if ( m_nBits > 64 - nShift ) {
			nValue |= SIRPackedIntegerEncoding::read64( m_pWords + ( nWord + 1 ) * 8 ) << ( 64 - nShift );
		}
		if ( m_nBits != 64 ) {
			nValue &= ( uint64_t( 1 ) << m_nBits ) - 1;
		}
		*pValue = nValue;
		return true;
	}

private:
	const uint8_t *m_pWords = nullptr;
	uint32_t m_nCount = 0;
	uint8_t m_nBits = 0;
};

class SIRPackedIntegerVector
{
public:
	explicit SIRPackedIntegerVector( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_Words( pAllocator )
	{
	}

	SIRPackedIntegerVector( const SIRPackedIntegerVector& ) = delete;
	SIRPackedIntegerVector& operator=( const SIRPackedIntegerVector& ) = delete;

	bool isSealed( void ) const noexcept
	{
		return m_bSealed;
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nCount;
	}

	uint8_t getBitWidth( void ) const noexcept
	{
		return m_nBits;
	}

	SIRContainerResult tryInitialize( uint8_t nBits, uint32_t nReserveCount = 0 )
	{
		if ( m_nBits != 0 || nBits == 0 || nBits > 64 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint32_t nWords = SIRPackedIntegerEncoding::countWords( nReserveCount, nBits );
		const SIRContainerResult eResult = m_Words.tryReserve( nWords );
		if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
			m_nBits = nBits;
		}
		return eResult;
	}

	SIRContainerResult tryAppend( uint64_t nValue )
	{
		if ( m_nBits == 0 || m_bSealed || m_nCount == UINT32_MAX ) {
			return SIRContainerResult::SIR_CONTAINER_CLOSED;
		}
		if ( m_nBits != 64 && nValue > ( ( uint64_t( 1 ) << m_nBits ) - 1 ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint64_t nBitOffset = static_cast<uint64_t>( m_nCount ) * m_nBits;
		const uint32_t iWord = static_cast<uint32_t>( nBitOffset >> 6 );
		const uint32_t nShift = static_cast<uint32_t>( nBitOffset & 63 );
		const uint32_t nNeeded = SIRPackedIntegerEncoding::countWords( static_cast<uint64_t>( m_nCount ) + 1, m_nBits );
		if ( nNeeded == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const SIRContainerResult eResult = m_Words.tryResize( nNeeded );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_Words[iWord] |= nValue << nShift;
		if ( m_nBits > 64 - nShift ) {
			m_Words[iWord + 1] |= nValue >> ( 64 - nShift );
		}
		++m_nCount;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool tryGet( uint32_t iIndex, uint64_t *pValue ) const noexcept
	{
		if ( iIndex >= m_nCount || pValue == nullptr ) {
			return false;
		}
		const uint64_t nBitOffset = static_cast<uint64_t>( iIndex ) * m_nBits;
		const uint32_t iWord = static_cast<uint32_t>( nBitOffset >> 6 );
		const uint32_t nShift = static_cast<uint32_t>( nBitOffset & 63 );
		uint64_t nValue = m_Words[iWord] >> nShift;
		if ( m_nBits > 64 - nShift ) {
			nValue |= m_Words[iWord + 1] << ( 64 - nShift );
		}
		if ( m_nBits != 64 ) {
			nValue &= ( uint64_t( 1 ) << m_nBits ) - 1;
		}
		*pValue = nValue;
		return true;
	}

	SIRContainerResult seal( void ) noexcept
	{
		if ( m_nBits == 0 || m_bSealed ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		m_bSealed = true;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	uint64_t getEncodedBytes( void ) const noexcept
	{
		return SIRPackedIntegerEncoding::HEADER_BYTES + static_cast<uint64_t>( m_Words.getSize() ) * 8;
	}

	SIRContainerResult tryEncode( uint8_t *pBytes, uint64_t nBytes ) const noexcept
	{
		if ( !m_bSealed || pBytes == nullptr || nBytes != getEncodedBytes() ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRPackedIntegerEncoding::write32( pBytes, SIRPackedIntegerEncoding::MAGIC );
		pBytes[4] = 1;
		pBytes[5] = m_nBits;
		pBytes[6] = 0;
		pBytes[7] = 0;
		SIRPackedIntegerEncoding::write32( pBytes + 8, m_nCount );
		SIRPackedIntegerEncoding::write32( pBytes + 12, m_Words.getSize() );
		for ( uint32_t i = 0; i < m_Words.getSize(); ++i ) {
			SIRPackedIntegerEncoding::write64( pBytes + SIRPackedIntegerEncoding::HEADER_BYTES + static_cast<uint64_t>( i ) * 8,
			                                   m_Words[i] );
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

private:
	SIRVector<uint64_t> m_Words;
	uint8_t m_nBits = 0;
	uint32_t m_nCount = 0;
	bool m_bSealed = false;
};

#endif // __SIR_PACKEDINTEGERVECTOR_H__

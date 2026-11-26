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

#ifndef __SIR_PACKEDSTRING_H__
#define __SIR_PACKEDSTRING_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRStringOps.h>
#include <Core/Strings/SIRStringSIMD.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Restricted alphabet: A-Z, a-z, 0-9, underscore, hyphen. Four low bits store length.
// All remaining bits are canonical (unused bits remain zero). No separate length field.
template <typename TWord>
class SIRPackedString final
{
public:
	explicit SIRPackedString() noexcept = default;
	explicit SIRPackedString( const SIRPackedString& ) noexcept = default;
	explicit SIRPackedString( SIRPackedString&& ) noexcept = default;
	~SIRPackedString() noexcept = default;
	SIRPackedString& operator=( const SIRPackedString& ) noexcept = default;
	SIRPackedString& operator=( SIRPackedString&& ) noexcept = default;

	[[nodiscard]] constexpr bool operator==( const SIRPackedString& other ) const noexcept
	{
		return m_nData == other.m_nData;
	}

	[[nodiscard]] constexpr uint32_t getCapacity( void ) const noexcept
	{
		return CAPACITY;
	}

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		return static_cast<uint32_t>( m_nData & 15u );
	}

	[[nodiscard]] constexpr TWord getPacked( void ) const noexcept
	{
		return m_nData;
	}

	[[nodiscard]] constexpr bool startsWith( const SIRPackedString& prefix ) const noexcept
	{
		const uint32_t nLength = prefix.getByteLength();
		if ( nLength > getByteLength() ) {
			return false;
		}
		const uint64_t nMask = ( uint64_t{ 1 } << ( nLength * 6 ) ) - 1;
		return ( ( static_cast<uint64_t>( m_nData ) >> 4 ) & nMask ) == ( static_cast<uint64_t>( prefix.m_nData ) >> 4 );
	}

	[[nodiscard]] constexpr SIRContainerResult tryReplace( uint32_t iByte, char nByte ) noexcept
	{
		const uint32_t nCode = encode( nByte );
		if ( iByte >= getByteLength() || nCode >= 64 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint32_t nShift = 4 + iByte * 6;
		m_nData = static_cast<TWord>( ( static_cast<uint64_t>( m_nData ) & ~( uint64_t{ 63 } << nShift ) ) | ( static_cast<uint64_t>( nCode ) << nShift ) );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	[[nodiscard]] constexpr SIRContainerResult tryConcat( const SIRPackedString& suffix ) noexcept
	{
		const uint32_t nLength = getByteLength();
		const uint32_t nSuffix = suffix.getByteLength();
		if ( nSuffix > CAPACITY - nLength ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		const uint64_t nPayload = static_cast<uint64_t>( suffix.m_nData ) >> 4;
		// Shift payload separately: empty suffix may follow a full 64-bit encoding.
		if ( nSuffix != 0 ) {
			m_nData = static_cast<TWord>( static_cast<uint64_t>( m_nData ) | ( nPayload << ( 4 + nLength * 6 ) ) );
		}
		m_nData = static_cast<TWord>( static_cast<uint64_t>( m_nData ) + nSuffix );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	[[nodiscard]] constexpr SIRContainerResult tryTruncate( uint32_t nLength ) noexcept
	{
		if ( nLength > getByteLength() ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		const uint64_t nMask = ( uint64_t{ 1 } << ( nLength * 6 ) ) - 1;
		m_nData = static_cast<TWord>( ( ( static_cast<uint64_t>( m_nData ) >> 4 ) & nMask ) << 4 | nLength );
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	[[nodiscard]] SIRContainerResult tryAssign( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( char nByte ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;

private:
	[[nodiscard]] static constexpr uint32_t encode( char nByte ) noexcept;
	[[nodiscard]] static constexpr char decode( uint32_t nCode ) noexcept;
	static_assert( std::is_integral<TWord>::value && std::is_unsigned<TWord>::value, "Unsigned word required" );
	static_assert( sizeof( TWord ) == 2 || sizeof( TWord ) == 4 || sizeof( TWord ) == 8, "Use 16, 32 or 64 bit words" );
	static constexpr uint32_t CAPACITY = ( sizeof( TWord ) * 8 - 4 ) / 6;
	TWord m_nData{};
};

template <typename TWord>
constexpr uint32_t SIRPackedString<TWord>::encode( char nByte ) noexcept
{
	if ( nByte >= 'A' && nByte <= 'Z' ) {
		return static_cast<uint32_t>( nByte - 'A' );
	}
	if ( nByte >= 'a' && nByte <= 'z' ) {
		return static_cast<uint32_t>( nByte - 'a' ) + 26;
	}
	if ( nByte >= '0' && nByte <= '9' ) {
		return static_cast<uint32_t>( nByte - '0' ) + 52;
	}
	if ( nByte == '_' ) {
		return 62;
	}
	return nByte == '-' ? 63 : 64;
}

template <typename TWord>
constexpr char SIRPackedString<TWord>::decode( uint32_t nCode ) noexcept
{
	if ( nCode < 26 ) {
		return static_cast<char>( 'A' + nCode );
	}
	if ( nCode < 52 ) {
		return static_cast<char>( 'a' + nCode - 26 );
	}
	if ( nCode < 62 ) {
		return static_cast<char>( '0' + nCode - 52 );
	}
	return nCode == 62 ? '_' : '-';
}

template <typename TWord>
SIRContainerResult SIRPackedString<TWord>::tryAssign( sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes > CAPACITY ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	uint64_t nPacked = view.nBytes;
	uint32_t iByte = 0;
	if ( view.nBytes >= 8 ) {
		uint64_t nCodes = 0;
		for ( ; iByte < 8; ++iByte ) {
			const uint32_t nCode = encode( view.pBytes[iByte] );
			if ( nCode >= 64 ) {
				return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
			}
			nCodes |= static_cast<uint64_t>( nCode ) << ( iByte * 8 );
		}
		nPacked |= SIRStringSIMD::packCodes( nCodes ) << 4;
	}
	for ( ; iByte < view.nBytes; ++iByte ) {
		const uint32_t nCode = encode( view.pBytes[iByte] );
		if ( nCode == 64 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		nPacked |= static_cast<uint64_t>( nCode ) << ( 4 + iByte * 6 );
	}
	m_nData = static_cast<TWord>( nPacked );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <typename TWord>
SIRContainerResult SIRPackedString<TWord>::tryAppend( char nByte ) noexcept
{
	const uint32_t nLength = getByteLength();
	if ( nLength == CAPACITY ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const uint32_t nCode = encode( nByte );
	if ( nCode == 64 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	m_nData = static_cast<TWord>( ( static_cast<uint64_t>( m_nData ) | ( static_cast<uint64_t>( nCode ) << ( 4 + nLength * 6 ) ) ) + 1 );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <typename TWord>
SIRContainerResult SIRPackedString<TWord>::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t nLength = getByteLength();
	if ( nCapacity <= nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	uint64_t nPacked = static_cast<uint64_t>( m_nData ) >> 4;
	uint32_t iByte = 0;
	if ( nLength >= 8 ) {
		const uint64_t nCodes = SIRStringSIMD::unpackCodes( nPacked );
		for ( ; iByte < 8; ++iByte ) {
			pszOutput[iByte] = decode( static_cast<uint32_t>( ( nCodes >> ( iByte * 8 ) ) & 63 ) );
		}
		nPacked >>= 48;
	}
	for ( ; iByte < nLength; ++iByte ) {
		pszOutput[iByte] = decode( static_cast<uint32_t>( nPacked & 63 ) );
		nPacked >>= 6;
	}
	pszOutput[nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

// Bijective radix representation. Zero means empty, digits are 1..radix.
// Supports restricted upper-case alphanumeric or the existing 64-symbol alphabet.
// Variable-length capacity depends on the encoded value; no separate length metadata.
template <typename TWord, uint32_t nRadix = 36>
class SIRRadixString final
{
public:
	explicit SIRRadixString() noexcept = default;
	explicit SIRRadixString( const SIRRadixString& ) noexcept = default;
	explicit SIRRadixString( SIRRadixString&& ) noexcept = default;
	~SIRRadixString() noexcept = default;
	SIRRadixString& operator=( const SIRRadixString& ) noexcept = default;
	SIRRadixString& operator=( SIRRadixString&& ) noexcept = default;

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		TWord nValue = m_nValue;
		uint32_t nLength = 0;
		while ( nValue != 0 ) {
			nValue = static_cast<TWord>( ( nValue - 1 ) / nRadix );
			++nLength;
		}
		return nLength;
	}

	[[nodiscard]] constexpr TWord getPacked( void ) const noexcept
	{
		return m_nValue;
	}

	[[nodiscard]] constexpr bool operator==( const SIRRadixString& other ) const noexcept
	{
		return m_nValue == other.m_nValue;
	}

	[[nodiscard]] SIRContainerResult tryAssign( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( char nByte ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;

private:
	static_assert( std::is_integral<TWord>::value && std::is_unsigned<TWord>::value, "Unsigned word required" );
	static_assert( sizeof( TWord ) == 2 || sizeof( TWord ) == 4 || sizeof( TWord ) == 8, "Use 16, 32 or 64 bit words" );
	static_assert( nRadix == 36 || nRadix == 64, "Unsupported alphabet" );
	static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz_-";
	TWord m_nValue{};
};

template <typename TWord, uint32_t nRadix>
SIRContainerResult SIRRadixString<TWord, nRadix>::tryAppend( char nByte ) noexcept
{
	uint32_t iDigit = 0;
	while ( iDigit < nRadix && ALPHABET[iDigit] != nByte ) {
		++iDigit;
	}
	if ( iDigit == nRadix ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint64_t nMax = static_cast<uint64_t>( static_cast<TWord>( ~TWord{ 0 } ) );
	const uint64_t nDigit = iDigit + 1;
	if ( m_nValue > ( nMax - nDigit ) / nRadix ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	m_nValue = static_cast<TWord>( static_cast<uint64_t>( m_nValue ) * nRadix + nDigit );
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <typename TWord, uint32_t nRadix>
SIRContainerResult SIRRadixString<TWord, nRadix>::tryAssign( sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	SIRRadixString candidate;
	for ( uint32_t iByte = 0; iByte < view.nBytes; ++iByte ) {
		const SIRContainerResult eResult = candidate.tryAppend( view.pBytes[iByte] );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
	}
	m_nValue = candidate.m_nValue;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <typename TWord, uint32_t nRadix>
SIRContainerResult SIRRadixString<TWord, nRadix>::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	const uint32_t nLength = getByteLength();
	if ( nCapacity <= nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	pszOutput[nLength] = '\0';
	uint32_t iByte = nLength;
	TWord nValue = m_nValue;
	while ( nValue != 0 ) {
		--nValue;
		pszOutput[--iByte] = ALPHABET[nValue % nRadix];
		nValue = static_cast<TWord>( nValue / nRadix );
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}
#endif // __SIR_PACKEDSTRING_H__

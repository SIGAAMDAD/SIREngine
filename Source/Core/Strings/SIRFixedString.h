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

#ifndef __SIR_FIXEDSTRING_H__
#define __SIR_FIXEDSTRING_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRStringOps.h>

// Capacity counts payload bytes; one additional byte is reserved for the terminator.
template <uint32_t nCapacity>
class SIRFixedString final
{
public:
	constexpr explicit SIRFixedString( void ) noexcept;
	constexpr explicit SIRFixedString( const SIRFixedString& other ) noexcept;
	constexpr explicit SIRFixedString( SIRFixedString&& other ) noexcept;
	~SIRFixedString() noexcept = default;

	SIRFixedString& operator=( const SIRFixedString& other ) noexcept;
	SIRFixedString& operator=( SIRFixedString&& other ) noexcept;

	[[nodiscard]] constexpr uint32_t getCapacity( void ) const noexcept
	{
		return nCapacity;
	}

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		if constexpr ( nCapacity <= 255 ) {
			return nCapacity - static_cast<uint8_t>( m_szData[nCapacity] );
		}
		uint32_t nLength = 0;
		for ( uint32_t iByte = 0; iByte < LENGTH_BYTES; ++iByte ) {
			nLength |= static_cast<uint32_t>( static_cast<uint8_t>( m_szData[static_cast<size_t>( nCapacity ) + 1 + iByte] ) ) << ( iByte * 8 );
		}
		return nLength;
	}

	[[nodiscard]] constexpr const char *cStr( void ) const noexcept
	{
		return m_szData;
	}

	[[nodiscard]] constexpr sirStringView_t getView( void ) const noexcept
	{
		return { m_szData, getByteLength() };
	}

	[[nodiscard]] SIRContainerResult tryAssign( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( sirStringView_t view ) noexcept;
	void clear( void ) noexcept;

private:
	static_assert( nCapacity < UINT32_MAX, "Terminator must fit" );
	static constexpr uint32_t LENGTH_BYTES = nCapacity <= 255 ? 0 : ( nCapacity <= 65535 ? 2 : 4 );
	static_assert( static_cast<uint64_t>( nCapacity ) + 1 + LENGTH_BYTES <= SIZE_MAX, "Storage must be addressable" );
	void setLength( uint32_t nLength ) noexcept;
	char m_szData[static_cast<size_t>( nCapacity ) + 1 + LENGTH_BYTES];
};

template <uint32_t nCapacity>
SIR_FORCEINLINE constexpr SIRFixedString<nCapacity>::SIRFixedString( void ) noexcept
{
	clear();
}

template <uint32_t nCapacity>
SIR_FORCEINLINE constexpr SIRFixedString<nCapacity>::SIRFixedString( const SIRFixedString& other ) noexcept
{
	const uint32_t nLength = other.getByteLength();
	memcpy( m_szData, other.m_szData, nLength + 1 );
	setLength( nLength );
}

template <uint32_t nCapacity>
SIRFixedString<nCapacity>::SIRFixedString( SIRFixedString&& other ) noexcept
    : SIRFixedString( static_cast<const SIRFixedString&>( other ) )
{
}

template <uint32_t nCapacity>
SIRFixedString<nCapacity>& SIRFixedString<nCapacity>::operator=( const SIRFixedString& other ) noexcept
{
	if ( this != &other ) {
		const uint32_t nLength = other.getByteLength();
		memcpy( m_szData, other.m_szData, nLength + 1 );
		setLength( nLength );
	}
	return *this;
}

template <uint32_t nCapacity>
SIRFixedString<nCapacity>& SIRFixedString<nCapacity>::operator=( SIRFixedString&& other ) noexcept
{
	return operator=( static_cast<const SIRFixedString&>( other ) );
}

template <uint32_t nCapacity>
void SIRFixedString<nCapacity>::setLength( uint32_t nLength ) noexcept
{
	if constexpr ( nCapacity <= 255 ) {
		m_szData[nCapacity] = static_cast<char>( nCapacity - nLength );
	} else {
		for ( uint32_t iByte = 0; iByte < LENGTH_BYTES; ++iByte ) {
			m_szData[static_cast<size_t>( nCapacity ) + 1 + iByte] = static_cast<char>( ( nLength >> ( iByte * 8 ) ) & 255 );
		}
	}
}

template <uint32_t nCapacity>
SIRContainerResult SIRFixedString<nCapacity>::tryAssign( sirStringView_t view ) noexcept
{
	const SIRContainerResult eResult = SIRStringOps::copyTerminated( view, m_szData, nCapacity + 1 );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		setLength( view.nBytes );
	}
	return eResult;
}

template <uint32_t nCapacity>
SIRContainerResult SIRFixedString<nCapacity>::tryAppend( sirStringView_t view ) noexcept
{
	const uint32_t nOld = getByteLength();
	const SIRContainerResult eResult = SIRStringOps::copyTerminated( view, m_szData + nOld, nCapacity - nOld + 1 );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		setLength( nOld + view.nBytes );
	}
	return eResult;
}

template <uint32_t nCapacity>
void SIRFixedString<nCapacity>::clear( void ) noexcept
{
	setLength( 0 );
	m_szData[0] = '\0';
}
#endif // __SIR_FIXEDSTRING_H__

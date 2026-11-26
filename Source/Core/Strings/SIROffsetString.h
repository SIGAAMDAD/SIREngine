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

#ifndef __SIR_OFFSETSTRING_H__
#define __SIR_OFFSETSTRING_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Offset and length are relative to the supplied immutable table. No pointer is persisted.
class SIROffsetString final
{
public:
	explicit constexpr SIROffsetString() noexcept = default;

	explicit constexpr SIROffsetString( uint32_t iOffset, uint32_t nBytes ) noexcept
	    : m_iOffset( iOffset ),
	      m_nBytes( nBytes )
	{
	}

	explicit SIROffsetString( const SIROffsetString& ) noexcept = default;
	explicit SIROffsetString( SIROffsetString&& ) noexcept = default;
	~SIROffsetString() noexcept = default;
	SIROffsetString& operator=( const SIROffsetString& ) noexcept = default;
	SIROffsetString& operator=( SIROffsetString&& ) noexcept = default;

	[[nodiscard]] constexpr uint32_t getOffset( void ) const noexcept
	{
		return m_iOffset;
	}

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		return m_nBytes;
	}

	[[nodiscard]] static SIRContainerResult tryFromBoundaries( uint32_t iBegin, uint32_t iEnd, SIROffsetString& output ) noexcept;
	[[nodiscard]] SIRContainerResult tryResolve( sirStringView_t table, sirStringView_t& view ) const noexcept;

private:
	uint32_t m_iOffset{};
	uint32_t m_nBytes{};
};

static_assert( sizeof( SIROffsetString ) == 8, "Offset string must remain eight bytes" );

// Four-byte catalog-relative reference; zero is a valid empty reference.
template <uint32_t nLengthBits>
class SIRPackedOffsetString final
{
public:
	explicit SIRPackedOffsetString() noexcept = default;
	explicit SIRPackedOffsetString( const SIRPackedOffsetString& ) noexcept = default;
	explicit SIRPackedOffsetString( SIRPackedOffsetString&& ) noexcept = default;
	~SIRPackedOffsetString() noexcept = default;
	SIRPackedOffsetString& operator=( const SIRPackedOffsetString& ) noexcept = default;
	SIRPackedOffsetString& operator=( SIRPackedOffsetString&& ) noexcept = default;

	[[nodiscard]] constexpr uint32_t getOffset( void ) const noexcept
	{
		return m_nPacked >> nLengthBits;
	}

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		return m_nPacked & LENGTH_MASK;
	}

	[[nodiscard]] constexpr SIRContainerResult tryAssign( uint32_t iOffset, uint32_t nBytes ) noexcept
	{
		if ( nBytes > LENGTH_MASK || iOffset > ( UINT32_MAX >> nLengthBits ) ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		m_nPacked = ( iOffset << nLengthBits ) | nBytes;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	[[nodiscard]] SIRContainerResult tryResolve( sirStringView_t table, sirStringView_t& view ) const noexcept;

private:
	static_assert( nLengthBits > 0 && nLengthBits < 32, "Invalid packed offset width" );
	static constexpr uint32_t LENGTH_MASK = ( uint32_t{ 1 } << nLengthBits ) - 1;
	uint32_t m_nPacked{};
};

template <uint32_t nLengthBits>
SIRContainerResult SIRPackedOffsetString<nLengthBits>::tryResolve( sirStringView_t table, sirStringView_t& view ) const noexcept
{
	const SIROffsetString offset( getOffset(), getByteLength() );
	return offset.tryResolve( table, view );
}

#endif // __SIR_OFFSETSTRING_H__

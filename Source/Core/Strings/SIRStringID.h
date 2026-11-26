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

#ifndef __SIR_STRINGID_H__
#define __SIR_STRINGID_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Stable content hash, compatible with SIRStringPool::hashString (FNV-1a).
// Equality compares hashes, not canonical string identity. Verify bytes when identity matters.
class SIRStringID final
{
public:
	explicit constexpr SIRStringID() noexcept = default;

	explicit constexpr SIRStringID( uint64_t nHash ) noexcept
	    : m_nHash( nHash )
	{
	}

	explicit SIRStringID( const SIRStringID& ) noexcept = default;
	explicit SIRStringID( SIRStringID&& ) noexcept = default;
	~SIRStringID() noexcept = default;
	SIRStringID& operator=( const SIRStringID& ) noexcept = default;
	SIRStringID& operator=( SIRStringID&& ) noexcept = default;

	[[nodiscard]] constexpr bool operator==( const SIRStringID& other ) const noexcept
	{
		return m_nHash == other.m_nHash;
	}

	[[nodiscard]] constexpr uint64_t getHash( void ) const noexcept
	{
		return m_nHash;
	}

	template <uint32_t nCount>
	[[nodiscard]] static constexpr SIRStringID fromLiteral( const char ( &szText )[nCount] ) noexcept
	{
		return SIRStringID( hashBytes( szText, nCount - 1 ) );
	}

	[[nodiscard]] static constexpr uint64_t hashBytes( const char *pBytes, uint32_t nBytes, uint64_t nSeed = 14695981039346656037ULL ) noexcept
	{
		for ( uint32_t iByte = 0; iByte < nBytes; ++iByte ) {
			nSeed = ( nSeed ^ static_cast<uint8_t>( pBytes[iByte] ) ) * 1099511628211ULL;
		}
		return nSeed;
	}

	// Inputs and outputs must not overlap; hashes are independent stable FNV-1a values.
	[[nodiscard]] static SIRContainerResult tryHashBatch( const sirStringView_t *pInputs, uint32_t nCount, uint64_t *pHashes ) noexcept;
	[[nodiscard]] SIRContainerResult tryAppend( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryAssign( sirStringView_t view ) noexcept;

private:
	uint64_t m_nHash{ 14695981039346656037ULL };
};

static_assert( sizeof( SIRStringID ) == 8, "String ID must remain eight bytes" );

#endif // __SIR_STRINGID_H__

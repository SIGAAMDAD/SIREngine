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

#ifndef SIRENGINE_SIROFFSETPTR_H
#define SIRENGINE_SIROFFSETPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// Base-relative, bounded 32-bit offset suitable for immutable cooked blobs.
// Offset 0xffffffff is null. Not a memory owner; base and blob lifetime are external.
template <typename T>
class SIROffsetPtr
{
public:
	SIROffsetPtr( void ) noexcept : m_nOffset( UINT32_MAX )
	{
	}
	explicit SIROffsetPtr( uint32_t nOffset ) noexcept : m_nOffset( nOffset )
	{
	}

	uint32_t getOffset( void ) const noexcept
	{
		return m_nOffset;
	}
	bool isValid( void ) const noexcept
	{
		return m_nOffset != UINT32_MAX;
	}
	const T *resolve( const void *pBase, uint64_t nBytes ) const noexcept
	{
		if ( pBase == nullptr || !isValid() ) {
			return nullptr;
		}
		if ( ( uint64_t )m_nOffset > nBytes || sizeof( T ) > nBytes - m_nOffset ) {
			return nullptr;
		}
		const uintptr_t nAddress = ( uintptr_t )pBase + m_nOffset;
		if ( nAddress < ( uintptr_t )pBase || nAddress % alignof( T ) != 0 ) {
			return nullptr;
		}
		return ( const T * )nAddress;
	}
	void reset( void ) noexcept
	{
		m_nOffset = UINT32_MAX;
	}

private:
	uint32_t m_nOffset;
};

#endif // SIRENGINE_SIROFFSETPTR_H

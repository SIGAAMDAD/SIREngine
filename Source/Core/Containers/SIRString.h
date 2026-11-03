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

#ifndef __SIRENGINE_SIRSTRING_H__
#define __SIRENGINE_SIRSTRING_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRInlineVector.h>
#include <stdint.h>

typedef struct sirStringView_s
{
	const char *pBytes;
	uint32_t nBytes;
} sirStringView_t;

// UTF-8 byte storage; validation and Unicode normalization are separate concerns.
class SIRString
{
private:
	SIRInlineVector<char, 24> m_bytes;

public:
	explicit SIRString( const sirContainerAllocator_t *pAllocator ) : m_bytes( pAllocator )
	{
		m_bytes.tryEmplaceBack( '\0' );
	}

	uint32_t getByteLength( void ) const noexcept
	{
		return m_bytes.getSize() - 1;
	}
	const char *cStr( void ) const noexcept
	{
		return m_bytes.data();
	}
	sirStringView_t getView( void ) const noexcept
	{
		return { cStr(), getByteLength() };
	}

	sirContainerResult_t tryAppendBytes( const char *pBytes, uint32_t nBytes )
	{
		if ( nBytes == 0 ) {
			return SIR_CONTAINER_OK;
		}
		if ( pBytes == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t nOldLength = getByteLength();
		if ( ( uint64_t )nOldLength + nBytes + 1 > UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		uintptr_t nSource = ( uintptr_t )pBytes;
		uintptr_t nStart = ( uintptr_t )m_bytes.data();
		bool bAliased = nSource >= nStart && nSource - nStart < m_bytes.getSize();
		uint32_t iSource = bAliased ? ( uint32_t )( nSource - nStart ) : 0;
		if ( bAliased && ( uint64_t )iSource + nBytes > m_bytes.getSize() ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		sirContainerResult_t eResult = m_bytes.tryReserve( nOldLength + nBytes + 1 );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		if ( bAliased ) {
			pBytes = m_bytes.data() + iSource;
		}
		// Grow the constructed character range before copying overlapping bytes.
		for ( uint32_t iExtra = 0; iExtra < nBytes; ++iExtra ) {
			m_bytes.tryEmplaceBack( '\0' );
		}
		memmove( m_bytes.data() + nOldLength, pBytes, nBytes );
		m_bytes.data()[nOldLength + nBytes] = '\0';
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryAssignBytes( const char *pBytes, uint32_t nBytes )
	{
		if ( pBytes == nullptr && nBytes != 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nBytes == UINT32_MAX ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		uintptr_t nSource = ( uintptr_t )pBytes;
		uintptr_t nStart = ( uintptr_t )m_bytes.data();
		bool bAliased = pBytes != nullptr && nSource >= nStart && nSource - nStart < m_bytes.getSize();
		uint32_t iSource = bAliased ? ( uint32_t )( nSource - nStart ) : 0;
		if ( bAliased && ( uint64_t )iSource + nBytes > m_bytes.getSize() ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		sirContainerResult_t eResult = m_bytes.tryReserve( nBytes + 1 );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		if ( bAliased ) {
			pBytes = m_bytes.data() + iSource;
		}
		while ( m_bytes.getSize() < nBytes + 1 ) {
			m_bytes.tryEmplaceBack( '\0' );
		}
		if ( nBytes != 0 ) {
			memmove( m_bytes.data(), pBytes, nBytes );
		}
		m_bytes.data()[nBytes] = '\0';
		while ( m_bytes.getSize() > nBytes + 1 ) {
			m_bytes.tryPopBack();
		}
		return SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		m_bytes.clear();
		m_bytes.tryEmplaceBack( '\0' );
	}
};

#endif // __SIRENGINE_SIRSTRING_H__

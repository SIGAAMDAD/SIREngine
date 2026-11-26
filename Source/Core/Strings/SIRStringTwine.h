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

#ifndef __SIR_STRINGTWINE_H__
#define __SIR_STRINGTWINE_H__

#include <Core/SIRPCH.h>
#include <Core/Containers/SIRString.h>
#include <Core/Strings/SIRStringOps.h>
#include <Core/Strings/SIRStringID.h>
#include <Core/Platform/Shared/File/SIRPlatformFile.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Bounded, flat lazy concatenation. Fragments borrow immutable bytes until render.
// The output must not alias any fragment. One final copy per fragment, no temporary string.
template <uint32_t nFragments>
class SIRStringTwine final
{
public:
	explicit SIRStringTwine() noexcept = default;
	explicit SIRStringTwine( const SIRStringTwine& ) = delete;
	explicit SIRStringTwine( SIRStringTwine&& ) = delete;
	~SIRStringTwine() noexcept = default;
	SIRStringTwine& operator=( const SIRStringTwine& ) = delete;
	SIRStringTwine& operator=( SIRStringTwine&& ) = delete;

	[[nodiscard]] constexpr uint32_t getByteLength( void ) const noexcept
	{
		return m_nLength;
	}

	[[nodiscard]] SIRContainerResult tryAppend( sirStringView_t view ) noexcept;
	[[nodiscard]] SIRContainerResult tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept;
	[[nodiscard]] SIRContainerResult tryAppendContiguous( sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept;
	[[nodiscard]] uint64_t hash( void ) const noexcept;
	[[nodiscard]] SIRContainerResult visit( SIRContainerResult ( *pfnVisit )( sirStringView_t, void * ) noexcept, void *pContext ) const noexcept;
	// The sink consumes all descriptors synchronously. A platform I/O adapter may
	// submit them as scatter/gather input without materializing the concatenation.
	[[nodiscard]] SIRContainerResult visitBatch( SIRContainerResult ( *pfnVisit )( const sirStringView_t *, uint32_t, void * ) noexcept,
	                                             void *pContext ) const noexcept;
	[[nodiscard]] sirFileResult tryWriteTo( SIRPlatformFile& file, uint64_t& nTransferred ) const noexcept;
	void clear( void ) noexcept;

private:
	static_assert( nFragments > 0, "At least one fragment required" );
	sirStringView_t m_Fragments[nFragments]{};
	uint32_t m_nCount{};
	uint32_t m_nLength{};
};

template <uint32_t nFragments>
SIRContainerResult SIRStringTwine<nFragments>::tryAppend( sirStringView_t view ) noexcept
{
	if ( !SIRStringOps::isValid( view ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( view.nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( m_nCount == nFragments ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( view.nBytes > UINT32_MAX - m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	m_Fragments[m_nCount++] = view;
	m_nLength += view.nBytes;
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <uint32_t nFragments>
SIRContainerResult SIRStringTwine<nFragments>::tryCopyTo( char *pszOutput, uint32_t nCapacity ) const noexcept
{
	if ( pszOutput == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nCapacity <= m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const sirStringView_t output{ pszOutput, m_nLength + 1 };
	for ( uint32_t iFragment = 0; iFragment < m_nCount; ++iFragment ) {
		if ( SIRStringOps::overlaps( output, m_Fragments[iFragment] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	uint32_t iOutput = 0;
	for ( uint32_t iFragment = 0; iFragment < m_nCount; ++iFragment ) {
		const sirStringView_t fragment = m_Fragments[iFragment];
		memcpy( pszOutput + iOutput, fragment.pBytes, fragment.nBytes );
		iOutput += fragment.nBytes;
	}
	pszOutput[iOutput] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <uint32_t nFragments>
void SIRStringTwine<nFragments>::clear( void ) noexcept
{
	m_nLength = 0;
	m_nCount = 0;
}

template <uint32_t nFragments>
uint64_t SIRStringTwine<nFragments>::hash( void ) const noexcept
{
	uint64_t nHash = 14695981039346656037ULL;
	for ( uint32_t iFragment = 0; iFragment < m_nCount; ++iFragment ) {
		const sirStringView_t view = m_Fragments[iFragment];
		nHash = SIRStringID::hashBytes( view.pBytes, view.nBytes, nHash );
	}
	return nHash;
}

template <uint32_t nFragments>
SIRContainerResult SIRStringTwine<nFragments>::visit( SIRContainerResult ( *pfnVisit )( sirStringView_t, void * ) noexcept, void *pContext ) const noexcept
{
	if ( pfnVisit == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t iFragment = 0; iFragment < m_nCount; ++iFragment ) {
		const SIRContainerResult eResult = pfnVisit( m_Fragments[iFragment], pContext );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
	}
	return SIRContainerResult::SIR_CONTAINER_OK;
}

template <uint32_t nFragments>
SIRContainerResult SIRStringTwine<nFragments>::tryAppendContiguous( sirStringView_t backing, uint32_t iOffset, uint32_t nBytes ) noexcept
{
	if ( !SIRStringOps::isValid( backing ) || iOffset > backing.nBytes || nBytes > backing.nBytes - iOffset ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( nBytes == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_OK;
	}
	if ( nBytes > UINT32_MAX - m_nLength ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	if ( m_nCount != 0 ) {
		sirStringView_t& last = m_Fragments[m_nCount - 1];
		if ( last.nBytes <= iOffset && last.pBytes == backing.pBytes + iOffset - last.nBytes ) {
			last.nBytes += nBytes;
			m_nLength += nBytes;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
	}
	return tryAppend( { backing.pBytes + iOffset, nBytes } );
}

template <uint32_t nFragments>
SIRContainerResult SIRStringTwine<nFragments>::visitBatch( SIRContainerResult ( *pfnVisit )( const sirStringView_t *, uint32_t, void * ) noexcept,
                                                           void *pContext ) const noexcept
{
	if ( pfnVisit == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	return pfnVisit( m_Fragments, m_nCount, pContext );
}
template <uint32_t nFragments>
sirFileResult SIRStringTwine<nFragments>::tryWriteTo( SIRPlatformFile& file, uint64_t& nTransferred ) const noexcept
{
	return file.writeFragments( m_Fragments, m_nCount, &nTransferred );
}
#endif // __SIR_STRINGTWINE_H__

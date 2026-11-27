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

#ifndef __SIR_READONLYVECTORVIEW_H__
#define __SIR_READONLYVECTORVIEW_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <stddef.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <assert.h>
#include <stdint.h>

template <typename T>
class SIRReadOnlyVectorView
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

	using value_type = T;
	using difference_type = ptrdiff_t;
	using const_reference = const T&;
	using const_pointer = const T *;
	using const_iterator = const T *;
	using reference = const T&;
	using pointer = const T *;
	using iterator = pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	reverse_iterator rbegin( void ) const noexcept
	{
		return reverse_iterator( end() );
	}
	reverse_iterator rend( void ) const noexcept
	{
		return reverse_iterator( begin() );
	}
	const_reverse_iterator crbegin( void ) const noexcept
	{
		return const_reverse_iterator( cend() );
	}
	const_reverse_iterator crend( void ) const noexcept
	{
		return const_reverse_iterator( cbegin() );
	}


	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	const T *tryFront( void ) const noexcept
	{
		return tryGet( 0 );
	}

	const T *tryBack( void ) const noexcept
	{
		return empty() ? nullptr : tryGet( getSize() - 1 );
	}

	const T& front( void ) const noexcept
	{
		assert( !empty() );
		return *tryFront();
	}

	const T& back( void ) const noexcept
	{
		assert( !empty() );
		return *tryBack();
	}

	explicit SIRReadOnlyVectorView( void ) noexcept = default;

	SIRReadOnlyVectorView( const T *pData, uint32_t nSize ) noexcept
	    : m_pData( pData ),
	      m_nSize( pData != nullptr ? nSize : 0 )
	{
		assert( pData != nullptr || nSize == 0 );
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	bool isEmpty( void ) const noexcept
	{
		return m_nSize == 0;
	}

	const T *data( void ) const noexcept
	{
		return m_pData;
	}

	const T *begin( void ) const noexcept
	{
		return m_pData;
	}

	const T *end( void ) const noexcept
	{
		return m_pData != nullptr ? m_pData + m_nSize : nullptr;
	}

	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}

	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}

	SIRReadOnlyVectorView subview( uint32_t nOffset, uint32_t nCount ) const noexcept
	{
		if ( nOffset > m_nSize || nCount > m_nSize - nOffset ) {
			return {};
		}
		return { m_pData == nullptr ? nullptr : m_pData + nOffset, nCount };
	}

private:
	const T *m_pData = nullptr;
	uint32_t m_nSize = 0;
};

#endif // __SIR_READONLYVECTORVIEW_H__

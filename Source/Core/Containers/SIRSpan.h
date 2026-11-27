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

#ifndef __SIR_SPAN_H__
#define __SIR_SPAN_H__

#include <Core/SIRPCH.h>
#include <iterator>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <type_traits>
#include <assert.h>
#include <stddef.h>

template <typename T>
class SIRSpan
{
public:
	using size_type = size_t;

	constexpr size_t size( void ) const noexcept
	{
		return getSize();
	}

	constexpr bool empty( void ) const noexcept
	{
		return getSize() == 0;
	}

	using value_type = typename std::remove_cv<T>::type;
	using difference_type = ptrdiff_t;
	using pointer = T *;
	using reference = T&;
	using iterator = T *;
	using const_iterator = const T *;
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


	constexpr T *data( void ) const noexcept
	{
		return getData();
	}

	constexpr iterator begin( void ) const noexcept
	{
		return data();
	}

	constexpr iterator end( void ) const noexcept
	{
		return data() == nullptr ? nullptr : data() + size();
	}

	constexpr const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	constexpr const_iterator cend( void ) const noexcept
	{
		return end();
	}

	constexpr T *tryGet( size_t iIndex ) const noexcept
	{
		return iIndex < size() ? data() + iIndex : nullptr;
	}

	constexpr T& front( void ) const noexcept
	{
		assert( !empty() );
		return data()[0];
	}

	constexpr T& back( void ) const noexcept
	{
		assert( !empty() );
		return data()[size() - 1];
	}

	constexpr SIRSpan( void ) noexcept = default;
	constexpr SIRSpan( const SIRSpan& ) noexcept = default;
	constexpr SIRSpan( SIRSpan&& ) noexcept = default;

	constexpr explicit SIRSpan( T *pData, size_t nSize ) noexcept;

	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U ( * )[], T ( * )[]>>>
	SIR_FORCEINLINE constexpr SIRSpan( const SIRSpan<U>& span ) noexcept
	    : m_pData( span.getData() ),
	      m_nSize( span.getSize() )
	{
	}

	~SIRSpan() noexcept = default;

	SIRSpan& operator=( const SIRSpan& ) noexcept = default;
	SIRSpan& operator=( SIRSpan&& ) noexcept = default;

	constexpr T& operator[]( size_t nIndex ) const noexcept;
	constexpr T *getData( void ) const noexcept;

	constexpr size_t getSize( void ) const noexcept;
	constexpr size_t getSizeBytes( void ) const noexcept;
	constexpr bool isEmpty( void ) const noexcept;

	constexpr size_t sizeBytes( void ) const noexcept
	{
		return getSizeBytes();
	}
	constexpr T *tryFront( void ) const noexcept
	{
		return tryGet( 0 );
	}
	constexpr T *tryBack( void ) const noexcept
	{
		return empty() ? nullptr : tryGet( size() - 1 );
	}

	constexpr SIRSpan first( size_t nCount ) const noexcept
	{
		return nCount <= size() ? subSpan( 0, nCount ) : SIRSpan();
	}

	constexpr SIRSpan last( size_t nCount ) const noexcept
	{
		return nCount <= size() ? subSpan( size() - nCount, nCount ) : SIRSpan();
	}

	constexpr SIRSpan subspan( size_t nOffset, size_t nCount ) const noexcept
	{
		return subSpan( nOffset, nCount );
	}

	template <size_t N>
	constexpr SIRSpan( T ( &rArray )[N] ) noexcept
	    : m_pData( rArray ),
	      m_nSize( N )
	{
	}

	constexpr SIRSpan subSpan( size_t nOffset, size_t nCount ) const noexcept;

private:
	T *m_pData = nullptr;
	size_t m_nSize = 0;
};

template <typename T>
SIR_FORCEINLINE constexpr SIRSpan<T>::SIRSpan( T *pData, size_t nSize ) noexcept
    : m_pData( pData ),
      m_nSize( nSize )
{
}

template <typename T>
SIR_FORCEINLINE constexpr T& SIRSpan<T>::operator[]( size_t nIndex ) const noexcept
{
	assert( nIndex < m_nSize );
	return m_pData[nIndex];
}

template <typename T>
SIR_FORCEINLINE constexpr T *SIRSpan<T>::getData( void ) const noexcept
{
	return m_pData;
}

template <typename T>
SIR_FORCEINLINE constexpr size_t SIRSpan<T>::getSize( void ) const noexcept
{
	return m_nSize;
}

template <typename T>
SIR_FORCEINLINE constexpr bool SIRSpan<T>::isEmpty( void ) const noexcept
{
	return m_nSize == 0;
}

template <typename T>
constexpr size_t SIRSpan<T>::getSizeBytes( void ) const noexcept
{
	return m_nSize * sizeof( T );
}

template <typename T>
constexpr SIRSpan<T> SIRSpan<T>::subSpan( size_t nOffset, size_t nCount ) const noexcept
{
	if ( nOffset > m_nSize ) {
		return SIRSpan<T>();
	}

	const size_t nRemaining = m_nSize - nOffset;
	const size_t nNewSize = nCount > nRemaining ? nRemaining : nCount;

	return SIRSpan<T>( m_pData == nullptr ? nullptr : m_pData + nOffset, nNewSize );
}

#endif // __SIR_SPAN_H__

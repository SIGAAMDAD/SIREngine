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

#ifndef __SIR_ARRAY_H__
#define __SIR_ARRAY_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <type_traits>
#include <utility>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <assert.h>
#include <stddef.h>

template <typename T, size_t N>
class SIRArray
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

	constexpr bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	using value_type = T;
	using difference_type = ptrdiff_t;
	using const_reference = const T&;
	using const_pointer = const T *;
	using const_iterator = const T *;
	using reference = T&;
	using pointer = T *;
	using iterator = pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	reverse_iterator rbegin( void ) noexcept
	{
		return reverse_iterator( end() );
	}
	reverse_iterator rend( void ) noexcept
	{
		return reverse_iterator( begin() );
	}
	const_reverse_iterator rbegin( void ) const noexcept
	{
		return const_reverse_iterator( end() );
	}
	const_reverse_iterator rend( void ) const noexcept
	{
		return const_reverse_iterator( begin() );
	}
	const_reverse_iterator crbegin( void ) const noexcept
	{
		return rbegin();
	}
	const_reverse_iterator crend( void ) const noexcept
	{
		return rend();
	}


	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	T *tryFront( void ) noexcept
	{
		return tryGet( 0 );
	}

	T *tryBack( void ) noexcept
	{
		return empty() ? nullptr : tryGet( getSize() - 1 );
	}

	T& front( void ) noexcept
	{
		assert( !empty() );
		return *tryFront();
	}

	T& back( void ) noexcept
	{
		assert( !empty() );
		return *tryBack();
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

	constexpr size_t getSize( void ) const noexcept
	{
		return N;
	}

	constexpr size_t getInlineBytes( void ) const noexcept
	{
		return N * sizeof( T );
	}

	constexpr size_t capacity( void ) const noexcept
	{
		return N;
	}
	constexpr size_t max_size( void ) const noexcept
	{
		return N;
	}

	void swap( SIRArray& rOther ) noexcept( std::is_nothrow_swappable<T>::value )
	{
		using std::swap;
		for ( size_t iIndex = 0; iIndex < N; ++iIndex ) {
			swap( m_szData[iIndex], rOther.m_szData[iIndex] );
		}
	}

	void fill( const T& value ) noexcept( std::is_nothrow_copy_assignable<T>::value )
	{
		for ( size_t iIndex = 0; iIndex < N; ++iIndex ) {
			m_szData[iIndex] = value;
		}
	}

	T *data( void ) noexcept
	{
		return m_szData;
	}

	const T *data( void ) const noexcept
	{
		return m_szData;
	}

	T *tryGet( size_t iIndex ) noexcept
	{
		return iIndex < N ? m_szData + iIndex : nullptr;
	}

	const T *tryGet( size_t iIndex ) const noexcept
	{
		return iIndex < N ? m_szData + iIndex : nullptr;
	}

	T& operator[]( size_t iIndex ) noexcept
	{
		assert( iIndex < N );
		return m_szData[iIndex];
	}

	const T& operator[]( size_t iIndex ) const noexcept
	{
		assert( iIndex < N );
		return m_szData[iIndex];
	}

	T *begin( void ) noexcept
	{
		return m_szData;
	}

	T *end( void ) noexcept
	{
		return m_szData + N;
	}

	const T *begin( void ) const noexcept
	{
		return m_szData;
	}

	const T *end( void ) const noexcept
	{
		return m_szData + N;
	}

private:
	static_assert( N > 0, "Fixed array requires positive extent" );

	T m_szData[N] = {};
};

// Zero-sized arrays have no live elements or hidden dummy object.
template <typename T>
class SIRArray<T, 0>
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

	constexpr bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	using value_type = T;
	using difference_type = ptrdiff_t;
	using const_reference = const T&;
	using const_pointer = const T *;
	using const_iterator = const T *;
	using reference = T&;
	using pointer = T *;
	using iterator = pointer;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	reverse_iterator rbegin( void ) noexcept
	{
		return reverse_iterator( end() );
	}
	reverse_iterator rend( void ) noexcept
	{
		return reverse_iterator( begin() );
	}
	const_reverse_iterator rbegin( void ) const noexcept
	{
		return const_reverse_iterator( end() );
	}
	const_reverse_iterator rend( void ) const noexcept
	{
		return const_reverse_iterator( begin() );
	}
	const_reverse_iterator crbegin( void ) const noexcept
	{
		return rbegin();
	}
	const_reverse_iterator crend( void ) const noexcept
	{
		return rend();
	}

	constexpr size_t capacity( void ) const noexcept
	{
		return 0;
	}
	constexpr size_t max_size( void ) const noexcept
	{
		return 0;
	}
	void swap( SIRArray& ) noexcept {}

	const_iterator cbegin( void ) const noexcept
	{
		return begin();
	}

	const_iterator cend( void ) const noexcept
	{
		return end();
	}

	void fill( const T& ) noexcept
	{
		
	}

	constexpr size_t getSize( void ) const noexcept
	{
		return 0;
	}

	constexpr size_t getInlineBytes( void ) const noexcept
	{
		return 0;
	}

	T *data( void ) noexcept
	{
		return nullptr;
	}

	const T *data( void ) const noexcept
	{
		return nullptr;
	}

	T *tryGet( size_t ) noexcept
	{
		return nullptr;
	}

	const T *tryGet( size_t ) const noexcept
	{
		return nullptr;
	}

	T *begin( void ) noexcept
	{
		return nullptr;
	}

	const T *begin( void ) const noexcept
	{
		return nullptr;
	}

	T *end( void ) noexcept
	{
		return nullptr;
	}

	const T *end( void ) const noexcept
	{
		return nullptr;
	}
};

#endif // __SIR_ARRAY_H__

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

#ifndef __SIR_STATICVECTOR_H__
#define __SIR_STATICVECTOR_H__

#include <Core/SIRPCH.h>
#include <iterator>
#include <stddef.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Containers/SIRWriteOnlyVectorView.h>
#include <assert.h>
#include <utility>

template <typename T, uint32_t N>
class SIRStaticVector
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

	bool isEmpty( void ) const noexcept
	{
		return empty();
	}

	constexpr size_type capacity( void ) const noexcept
	{
		return getCapacity();
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


	T *begin( void ) noexcept
	{
		return data();
	}

	T *end( void ) noexcept
	{
		return data() == nullptr ? nullptr : data() + getSize();
	}

	const T *begin( void ) const noexcept
	{
		return data();
	}

	const T *end( void ) const noexcept
	{
		return data() == nullptr ? nullptr : data() + getSize();
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

	SIRContainerResult tryResize( uint32_t nSize )
	{
		if ( nSize > N ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		while ( getSize() > nSize ) {
			tryPopBack();
		}
		const uint32_t nOriginal = getSize();
		try {
			while ( getSize() < nSize ) {
				tryEmplaceBack();
			}
		} catch ( ... ) {
			while ( getSize() > nOriginal ) {
				tryPopBack();
			}
			throw;
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	explicit SIRStaticVector( void ) noexcept = default;

	~SIRStaticVector() noexcept
	{
		clear();
	}

	SIRStaticVector( const SIRStaticVector& ) = delete;
	SIRStaticVector& operator=( const SIRStaticVector& ) = delete;
	SIRStaticVector( SIRStaticVector&& ) = delete;
	SIRStaticVector& operator=( SIRStaticVector&& ) = delete;

	SIRReadOnlyVectorView<T> getReadOnlyView( void ) const noexcept
	{
		return { data(), m_nSize };
	}

	SIRWriteOnlyVectorView<T> getWriteOnlyView( void ) noexcept
	{
		return { data(), m_nSize };
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	constexpr uint32_t getCapacity( void ) const noexcept
	{
		return N;
	}

	T *data( void ) noexcept
	{
		return slot( 0 );
	}

	const T *data( void ) const noexcept
	{
		return slot( 0 );
	}

	T *tryGet( uint32_t iIndex ) noexcept
	{
		return iIndex < m_nSize ? slot( iIndex ) : nullptr;
	}

	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? slot( iIndex ) : nullptr;
	}

	T& operator[]( uint32_t iIndex ) noexcept
	{
		assert( iIndex < m_nSize );
		return *slot( iIndex );
	}

	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < m_nSize );
		return *slot( iIndex );
	}

	SIRContainerResult tryReserve( uint32_t nCapacity ) noexcept
	{
		return nCapacity <= N ? SIRContainerResult::SIR_CONTAINER_OK : SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize == N ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		new ( slot( m_nSize ) ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryPushBack( const T& value )
	{
		return tryEmplaceBack( value );
	}

	SIRContainerResult tryPushBack( T&& value )
	{
		return tryEmplaceBack( std::move( value ) );
	}

	SIRContainerResult tryAppendRange( const T *pValues, uint32_t nCount )
	{
		if ( nCount != 0 && pValues == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( nCount > N - m_nSize )
		{
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		const uint32_t nBefore = m_nSize;
		try {
			for ( uint32_t iIndex = 0; iIndex < nCount; ++iIndex ) {
				tryEmplaceBack( pValues[iIndex] );
			}
		} catch ( ... ) {
			while ( m_nSize > nBefore )
			{
				tryPopBack();
			}
			throw;
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryEraseOrdered( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Ordered erase requires noexcept move assignment" );
		if ( iIndex >= m_nSize )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		for ( uint32_t iMove = iIndex + 1; iMove < m_nSize; ++iMove ) {
			*slot( iMove - 1 ) = std::move( *slot( iMove ) );
		}
		return tryPopBack();
	}

	SIRContainerResult tryEraseSwap( uint32_t iIndex ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Swap erase requires noexcept move assignment" );
		if ( iIndex >= m_nSize )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( iIndex + 1 != m_nSize )
		{
			*slot( iIndex ) = std::move( *slot( m_nSize - 1 ) );
		}
		return tryPopBack();
	}

	SIRContainerResult tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		slot( m_nSize )->~T();
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void clear( void ) noexcept
	{
		while ( m_nSize != 0 ) {
			--m_nSize;
			slot( m_nSize )->~T();
		}
	}

	sirContainerFootprint_t getFootprint( void ) const noexcept
	{
		return { static_cast<uint64_t>( N * sizeof( T ) ), 0, 0, 0, static_cast<uint64_t>( m_nSize * sizeof( T ) ) };
	}

private:
	static_assert( N > 0, "Fixed container requires positive capacity" );
	alignas( T ) unsigned char m_szStorage[sizeof( T ) * N];
	uint32_t m_nSize = 0;

	T *slot( uint32_t iIndex ) noexcept
	{
		return ( T * )( m_szStorage + static_cast<size_t>( iIndex * sizeof( T ) ) );
	}

	const T *slot( uint32_t iIndex ) const noexcept
	{
		return ( const T * )( m_szStorage + static_cast<size_t>( iIndex * sizeof( T ) ) );
	}
};

#endif // __SIR_STATICVECTOR_H__

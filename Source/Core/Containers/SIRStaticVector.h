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

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <Core/Containers/SIRReadOnlyVectorView.h>
#include <Core/Containers/SIRWriteOnlyVectorView.h>
#include <assert.h>
#include <new>
#include <type_traits>
#include <utility>

template <typename T, uint32_t N>
class SIRStaticVector
{
public:
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

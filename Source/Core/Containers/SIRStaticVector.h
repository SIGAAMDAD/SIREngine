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

#ifndef __SIRENGINE_SIRSTATICVECTOR_H__
#define __SIRENGINE_SIRSTATICVECTOR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVectorStorage.h>
#include <assert.h>
#include <new>
#include <type_traits>
#include <utility>

template <typename T, uint32_t N>
class SIRStaticVector
{
private:
	static_assert( N > 0, "Fixed container requires positive capacity" );
	alignas( T ) unsigned char m_szStorage[sizeof( T ) * N];
	uint32_t m_nSize = 0;

	T *slot( uint32_t iIndex ) noexcept
	{
		return ( T * )( m_szStorage + ( size_t )iIndex * sizeof( T ) );
	}
	const T *slot( uint32_t iIndex ) const noexcept
	{
		return ( const T * )( m_szStorage + ( size_t )iIndex * sizeof( T ) );
	}

public:
	SIRStaticVector( void ) noexcept = default;
	~SIRStaticVector( void ) noexcept
	{
		clear();
	}
	SIRStaticVector( const SIRStaticVector& ) = delete;
	SIRStaticVector& operator=( const SIRStaticVector& ) = delete;
	SIRStaticVector( SIRStaticVector&& ) = delete;
	SIRStaticVector& operator=( SIRStaticVector&& ) = delete;

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

	sirContainerResult_t tryReserve( uint32_t nCapacity ) noexcept
	{
		return nCapacity <= N ? SIR_CONTAINER_OK : SIR_CONTAINER_CAPACITY_EXCEEDED;
	}

	template <typename... Args>
	sirContainerResult_t tryEmplaceBack( Args&&...args )
	{
		if ( m_nSize == N ) {
			return SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		new ( slot( m_nSize ) ) T( std::forward<Args>( args )... );
		++m_nSize;
		return SIR_CONTAINER_OK;
	}

	sirContainerResult_t tryPushBack( const T& value )
	{
		return tryEmplaceBack( value );
	}
	sirContainerResult_t tryPushBack( T&& value )
	{
		return tryEmplaceBack( std::move( value ) );
	}

	sirContainerResult_t tryPopBack( void ) noexcept
	{
		if ( m_nSize == 0 ) {
			return SIR_CONTAINER_EMPTY;
		}
		--m_nSize;
		slot( m_nSize )->~T();
		return SIR_CONTAINER_OK;
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
		return { ( uint64_t )N * sizeof( T ), 0, 0, 0, ( uint64_t )m_nSize * sizeof( T ) };
	}
};

#endif // __SIRENGINE_SIRSTATICVECTOR_H__

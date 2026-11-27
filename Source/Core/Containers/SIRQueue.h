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

#ifndef __SIR_QUEUE_H__
#define __SIR_QUEUE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRRingBuffer.h>
#include <utility>

// Bounded FIFO adapter. Capacity is explicit and fixed after initialization.
template <typename T>
class SIRQueue
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

	size_type capacity( void ) const noexcept
	{
		return getCapacity();
	}

	using value_type = T;

	bool isFull( void ) const noexcept
	{
		return m_ring.isFull();
	}

	T *back( void ) noexcept
	{
		return m_ring.back();
	}

	const T *back( void ) const noexcept
	{
		return m_ring.back();
	}

	explicit SIRQueue( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_ring( pAllocator )
	{
	}

	SIRContainerResult tryInitialize( uint32_t nCapacity ) noexcept
	{
		return m_ring.tryInitialize( nCapacity );
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_ring.getSize();
	}

	uint32_t getCapacity( void ) const noexcept
	{
		return m_ring.getCapacity();
	}

	const T *front( void ) const noexcept
	{
		return m_ring.front();
	}

	T *front( void ) noexcept
	{
		return m_ring.front();
	}

	template <typename... Args>
	SIRContainerResult tryEmplace( Args&&...args )
	{
		return m_ring.tryEmplaceBack( std::forward<Args>( args )... );
	}

	SIRContainerResult tryPush( const T& value )
	{
		return m_ring.tryPushBack( value );
	}

	SIRContainerResult tryPush( T&& value )
	{
		return m_ring.tryPushBack( std::move( value ) );
	}

	SIRContainerResult tryPushMany( const T *pItems, uint32_t nCount ) noexcept
	{
		return m_ring.tryPushBackMany( pItems, nCount );
	}

	SIRContainerResult tryPopMany( T *pOutput, uint32_t nCount ) noexcept
	{
		return m_ring.tryPopFrontMany( pOutput, nCount );
	}

	SIRContainerResult tryPop( void ) noexcept
	{
		return m_ring.tryPopFront();
	}

	void clearAndRelease( void ) noexcept
	{
		m_ring.clearAndRelease();
	}

	// Non-owning peek; nullptr signifies an empty adapter.
	T *tryPeek( void ) noexcept
	{
		return front();
	}
	const T *tryPeek( void ) const noexcept
	{
		return front();
	}

	SIRContainerResult tryPopValue( T *pOutput ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Pop into value requires noexcept move assignment" );
		if ( pOutput == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		T *pValue = front();
		if ( pValue == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		*pOutput = std::move( *pValue );
		return tryPop();
	}

	void clear( void ) noexcept
	{
		m_ring.clear();
	}

private:
	SIRRingBuffer<T> m_ring;
};

#endif // __SIR_QUEUE_H__

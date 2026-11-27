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

#ifndef __SIR_STACK_H__
#define __SIR_STACK_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <utility>

template <typename T>
class SIRStack
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

	using value_type = T;

	T *top( void ) noexcept
	{
		return peek();
	}

	const T *top( void ) const noexcept
	{
		return peek();
	}

	size_type getCapacity( void ) const noexcept
	{
		return m_elements.getCapacity();
	}

	size_type capacity( void ) const noexcept
	{
		return getCapacity();
	}

	SIRContainerResult tryReserve( uint32_t nCapacity )
	{
		return m_elements.tryReserve( nCapacity );
	}

	explicit SIRStack( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_elements( pAllocator )
	{
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_elements.getSize();
	}

	const T *peek( void ) const noexcept
	{
		uint32_t nSize = m_elements.getSize();
		return nSize == 0 ? nullptr : m_elements.tryGet( nSize - 1 );
	}

	T *peek( void ) noexcept
	{
		uint32_t nSize = m_elements.getSize();
		return nSize == 0 ? nullptr : m_elements.tryGet( nSize - 1 );
	}

	template <typename... Args>
	SIRContainerResult tryEmplace( Args&&...args )
	{
		return m_elements.tryEmplaceBack( std::forward<Args>( args )... );
	}

	SIRContainerResult tryPush( const T& value )
	{
		return m_elements.tryPushBack( value );
	}

	SIRContainerResult tryPush( T&& value )
	{
		return m_elements.tryPushBack( std::move( value ) );
	}

	SIRContainerResult tryPop( void ) noexcept
	{
		return m_elements.tryPopBack();
	}

	SIRContainerResult tryShrinkToFit( void )
	{
		return m_elements.tryShrinkToFit();
	}

	// Non-owning peek; nullptr signifies an empty adapter.
	T *tryPeek( void ) noexcept
	{
		return top();
	}
	const T *tryPeek( void ) const noexcept
	{
		return top();
	}

	SIRContainerResult tryPopValue( T *pOutput ) noexcept
	{
		static_assert( std::is_nothrow_move_assignable<T>::value, "Pop into value requires noexcept move assignment" );
		if ( pOutput == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		T *pValue = top();
		if ( pValue == nullptr )
		{
			return SIRContainerResult::SIR_CONTAINER_EMPTY;
		}
		*pOutput = std::move( *pValue );
		return tryPop();
	}

	void clear( void ) noexcept
	{
		m_elements.clear();
	}

private:
	SIRVector<T> m_elements;
};

#endif // __SIR_STACK_H__

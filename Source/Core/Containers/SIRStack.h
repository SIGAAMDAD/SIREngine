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

#ifndef __SIRENGINE_SIRSTACK_H__
#define __SIRENGINE_SIRSTACK_H__

#include <SIRPCH.hpp>

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
private:
	SIRVector<T> m_elements;

public:
	explicit SIRStack( const sirContainerAllocator_t *pAllocator ) noexcept : m_elements( pAllocator )
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
	sirContainerResult_t tryEmplace( Args &&...args )
	{
		return m_elements.tryEmplaceBack( std::forward<Args>( args )... );
	}
	sirContainerResult_t tryPush( const T &value )
	{
		return m_elements.tryPushBack( value );
	}
	sirContainerResult_t tryPush( T &&value )
	{
		return m_elements.tryPushBack( std::move( value ) );
	}
	sirContainerResult_t tryPop( void ) noexcept
	{
		return m_elements.tryPopBack();
	}
	void clear( void ) noexcept
	{
		m_elements.clear();
	}
};

#endif // __SIRENGINE_SIRSTACK_H__

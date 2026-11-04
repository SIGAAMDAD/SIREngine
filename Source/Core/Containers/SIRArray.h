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

#ifndef __SIRENGINE_SIRARRAY_H__
#define __SIRENGINE_SIRARRAY_H__

#include <SIRPCH.hpp>

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
private:
	static_assert( N > 0, "Fixed array requires positive extent" );
	T m_szData[N] = {};

public:
	constexpr size_t getSize( void ) const noexcept
	{
		return N;
	}
	constexpr size_t getInlineBytes( void ) const noexcept
	{
		return N * sizeof( T );
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
};

// Zero-sized arrays have no live elements or hidden dummy object.
template <typename T>
class SIRArray<T, 0>
{
public:
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

#endif // __SIRENGINE_SIRARRAY_H__

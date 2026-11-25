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

#ifndef __SIR_COMPRESSEDFUNCTOR_H__
#define __SIR_COMPRESSEDFUNCTOR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>
#include <type_traits>
#include <utility>

// Discriminated empty-base storage: multiple functors with identical C++
// types remain legal as distinct base classes. Final functors use a member.
template <typename TFunctor, uint32_t nIndex, bool bEmpty = std::is_empty<TFunctor>::value && !std::is_final<TFunctor>::value>
class SIRCompressedFunctor
{
public:
	explicit SIRCompressedFunctor( void ) = default;

	explicit SIRCompressedFunctor( TFunctor&& rValue ) noexcept( std::is_nothrow_move_constructible<TFunctor>::value )
	    : m_Functor( std::move( rValue ) )
	{
	}

	TFunctor& getFunctor( void ) noexcept
	{
		return m_Functor;
	}

	const TFunctor& getFunctor( void ) const noexcept
	{
		return m_Functor;
	}

private:
	TFunctor m_Functor;
};

template <typename TFunctor, uint32_t nIndex>
class SIRCompressedFunctor<TFunctor, nIndex, true> : private TFunctor
{
public:
	explicit SIRCompressedFunctor( void ) = default;

	explicit SIRCompressedFunctor( TFunctor&& rValue ) noexcept( std::is_nothrow_move_constructible<TFunctor>::value )
	    : TFunctor( std::move( rValue ) )
	{
	}

	TFunctor& getFunctor( void ) noexcept
	{
		return *this;
	}

	const TFunctor& getFunctor( void ) const noexcept
	{
		return *this;
	}
};

#endif // __SIR_COMPRESSEDFUNCTOR_H__

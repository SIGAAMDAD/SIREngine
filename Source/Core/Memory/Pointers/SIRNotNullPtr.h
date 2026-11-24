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

#ifndef __SIR_NOTNULLPTR_H__
#define __SIR_NOTNULLPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdlib.h>
#include <cstddef>
#include <type_traits>

// Non-owning, non-null object pointer. The pointee's lifetime remains external.
template <typename T>
class SIRNotNullPtr
{
public:
	SIRNotNullPtr( void ) = delete;
	SIRNotNullPtr( std::nullptr_t ) = delete;
	explicit SIRNotNullPtr( T *pObject ) noexcept : m_pObject( pObject )
	{
		if ( m_pObject == nullptr ) {
			abort();
		}
	}
	template <typename U, typename std::enable_if<std::is_convertible<U *, T *>::value, int>::type = 0>
	SIRNotNullPtr( const SIRNotNullPtr<U>& rOther ) noexcept : m_pObject( rOther.get() )
	{
	}
	SIRNotNullPtr( const SIRNotNullPtr& ) noexcept = default;
	SIRNotNullPtr& operator=( const SIRNotNullPtr& ) noexcept = default;
	SIRNotNullPtr& operator=( std::nullptr_t ) = delete;

	T *get( void ) const noexcept
	{
		return m_pObject;
	}
	T *operator->( void ) const noexcept
	{
		return m_pObject;
	}
	T& operator*( void ) const noexcept
	{
		return *m_pObject;
	}

private:
	T *m_pObject;
};

#endif // __SIR_NOTNULLPTR_H__

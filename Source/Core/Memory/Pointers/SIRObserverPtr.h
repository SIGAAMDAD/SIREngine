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

#ifndef __SIR_OBSERVERPTR_H__
#define __SIR_OBSERVERPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Non-owning, nullable pointer. No expiration detection or ownership operations.
template <typename T>
class SIRObserverPtr
{
public:
	SIRObserverPtr( void ) noexcept : m_pObject( nullptr )
	{
	}
	explicit SIRObserverPtr( T *pObject ) noexcept : m_pObject( pObject )
	{
	}

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
	bool isValid( void ) const noexcept
	{
		return m_pObject != nullptr;
	}
	void reset( T *pObject = nullptr ) noexcept
	{
		m_pObject = pObject;
	}

private:
	T *m_pObject;
};

#endif // __SIR_OBSERVERPTR_H__

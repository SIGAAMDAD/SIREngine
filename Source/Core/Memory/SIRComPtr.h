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

#ifndef SIRENGINE_SIRCOMPTR_H
#define SIRENGINE_SIRCOMPTR_H

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <utility>

// Owns one COM interface reference. COM producers return an already retained
// interface; use attach() to adopt it and copyFrom() to acquire another claim.
// On Windows this requires IUnknown-compatible AddRef/Release methods.
template <typename T>
class SIRComPtr
{
public:
	SIRComPtr( void ) noexcept : m_pInterface( nullptr )
	{
	}
	SIRComPtr( const SIRComPtr& rOther ) noexcept : m_pInterface( rOther.m_pInterface )
	{
		addRef();
	}
	SIRComPtr( SIRComPtr&& rOther ) noexcept : m_pInterface( rOther.detach() )
	{
	}
	~SIRComPtr( void ) noexcept
	{
		reset();
	}
	SIRComPtr& operator=( const SIRComPtr& rOther ) noexcept
	{
		SIRComPtr copy( rOther );
		swap( copy );
		return *this;
	}
	SIRComPtr& operator=( SIRComPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pInterface = rOther.detach();
		}
		return *this;
	}
	T *get( void ) const noexcept
	{
		return m_pInterface;
	}
	T *operator->( void ) const noexcept
	{
		return m_pInterface;
	}
	explicit operator bool( void ) const noexcept
	{
		return m_pInterface != nullptr;
	}
	bool isValid( void ) const noexcept
	{
		return m_pInterface != nullptr;
	}
	void attach( T *pInterface ) noexcept
	{
		if ( m_pInterface == pInterface ) {
			// Adopt the new claim, releasing our existing one.
			if ( pInterface != nullptr ) {
				pInterface->Release();
			}
			return;
		}
		reset();
		m_pInterface = pInterface;
	}
	void copyFrom( T *pInterface ) noexcept
	{
		if ( pInterface != nullptr ) {
			pInterface->AddRef();
		}
		reset();
		m_pInterface = pInterface;
	}
	T *detach( void ) noexcept
	{
		T *pInterface = m_pInterface;
		m_pInterface = nullptr;
		return pInterface;
	}
	// Use only for an out-parameter that returns an owned COM reference.
	T **put( void ) noexcept
	{
		reset();
		return &m_pInterface;
	}
	void reset( void ) noexcept
	{
		T *pInterface = m_pInterface;
		m_pInterface = nullptr;
		if ( pInterface != nullptr ) {
			pInterface->Release();
		}
	}
	void swap( SIRComPtr& rOther ) noexcept
	{
		T *pInterface = m_pInterface;
		m_pInterface = rOther.m_pInterface;
		rOther.m_pInterface = pInterface;
	}

private:
	void addRef( void ) noexcept
	{
		if ( m_pInterface != nullptr ) {
			m_pInterface->AddRef();
		}
	}

	T *m_pInterface;
};

#endif // SIRENGINE_SIRCOMPTR_H

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

#ifndef __SIR_INTRUSIVEPTR_H__
#define __SIR_INTRUSIVEPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Intrusive hooks are supplied by the managed type's owning module:
// SIRIntrusiveAddRef( T *pObject ) noexcept;
// SIRIntrusiveRelease( T *pObject ) noexcept;
// A pointer stores only T*; its object owns the reference count.
template <typename T>
class SIRIntrusivePtr
{
public:
	SIRIntrusivePtr( void ) noexcept : m_pObject( nullptr )
	{
	}
	explicit SIRIntrusivePtr( T *pObject ) noexcept : m_pObject( pObject )
	{
		if ( m_pObject != nullptr ) {
			SIRIntrusiveAddRef( m_pObject );
		}
	}
	~SIRIntrusivePtr( void ) noexcept
	{
		reset();
	}
	SIRIntrusivePtr( const SIRIntrusivePtr& rOther ) noexcept : SIRIntrusivePtr( rOther.m_pObject )
	{
	}
	SIRIntrusivePtr( SIRIntrusivePtr&& rOther ) noexcept : m_pObject( rOther.detach() )
	{
	}
	SIRIntrusivePtr& operator=( const SIRIntrusivePtr& rOther ) noexcept
	{
		if ( this == &rOther ) {
			return *this;
		}
		SIRIntrusivePtr copy( rOther );
		swap( copy );
		return *this;
	}
	SIRIntrusivePtr& operator=( SIRIntrusivePtr&& rOther ) noexcept
	{
		if ( this == &rOther ) {
			return *this;
		}
		reset();
		m_pObject = rOther.detach();
		return *this;
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
	T *detach( void ) noexcept
	{
		T *pObject = m_pObject;
		m_pObject = nullptr;
		return pObject;
	}
	void reset( void ) noexcept
	{
		if ( m_pObject != nullptr ) {
			SIRIntrusiveRelease( m_pObject );
		}
		m_pObject = nullptr;
	}
	void swap( SIRIntrusivePtr& rOther ) noexcept
	{
		T *pObject = m_pObject;
		m_pObject = rOther.m_pObject;
		rOther.m_pObject = pObject;
	}

private:
	T *m_pObject;
};

#endif // __SIR_INTRUSIVEPTR_H__

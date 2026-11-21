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

#ifndef __SIRENGINE_SIRUNIQUEPTR_H__
#define __SIRENGINE_SIRUNIQUEPTR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <type_traits>
#include <utility>

// Empty deleters occupy no additional storage unless they are final.
template <typename TDeleter, bool bEmpty = std::is_empty<TDeleter>::value && !std::is_final<TDeleter>::value>
class SIRUniqueDeleterStorage
{
public:
	SIRUniqueDeleterStorage( void ) = default;
	explicit SIRUniqueDeleterStorage( const TDeleter& rDeleter ) : m_Deleter( rDeleter )
	{
	}
	explicit SIRUniqueDeleterStorage( TDeleter&& rDeleter ) : m_Deleter( std::move( rDeleter ) )
	{
	}
	TDeleter& getDeleter( void ) noexcept
	{
		return m_Deleter;
	}
	const TDeleter& getDeleter( void ) const noexcept
	{
		return m_Deleter;
	}

private:
	TDeleter m_Deleter;
};

template <typename TDeleter>
class SIRUniqueDeleterStorage<TDeleter, true> : private TDeleter
{
public:
	SIRUniqueDeleterStorage( void ) = default;
	explicit SIRUniqueDeleterStorage( const TDeleter& rDeleter ) : TDeleter( rDeleter )
	{
	}
	explicit SIRUniqueDeleterStorage( TDeleter&& rDeleter ) : TDeleter( std::move( rDeleter ) )
	{
	}
	TDeleter& getDeleter( void ) noexcept
	{
		return *this;
	}
	const TDeleter& getDeleter( void ) const noexcept
	{
		return *this;
	}
};

// Requires a caller-supplied allocator-specific deleter and never assumes
// global new/delete. The deleter operation must be noexcept.
template <typename T, typename TDeleter>
class SIRUniquePtr : private SIRUniqueDeleterStorage<TDeleter>
{
public:
	SIRUniquePtr( void ) noexcept( std::is_nothrow_default_constructible<TDeleter>::value ) = default;
	SIRUniquePtr( T *pObject, const TDeleter& rDeleter ) noexcept( std::is_nothrow_copy_constructible<TDeleter>::value )
	    : SIRUniqueDeleterStorage<TDeleter>( rDeleter ), m_pObject( pObject )
	{
	}
	SIRUniquePtr( T *pObject, TDeleter&& rDeleter ) noexcept( std::is_nothrow_move_constructible<TDeleter>::value )
	    : SIRUniqueDeleterStorage<TDeleter>( std::move( rDeleter ) ), m_pObject( pObject )
	{
	}
	~SIRUniquePtr( void ) noexcept
	{
		reset();
	}
	SIRUniquePtr( const SIRUniquePtr& ) = delete;
	SIRUniquePtr( SIRUniquePtr&& rOther ) noexcept( std::is_nothrow_move_constructible<TDeleter>::value )
	    : SIRUniqueDeleterStorage<TDeleter>( std::move( rOther.getDeleter() ) ), m_pObject( rOther.release() )
	{
	}
	SIRUniquePtr& operator=( const SIRUniquePtr& ) = delete;
	SIRUniquePtr& operator=( SIRUniquePtr&& rOther ) noexcept( std::is_nothrow_move_assignable<TDeleter>::value )
	{
		if ( this != &rOther ) {
			reset();
			this->getDeleter() = std::move( rOther.getDeleter() );
			m_pObject = rOther.release();
		}
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
	TDeleter& getDeleter( void ) noexcept
	{
		return SIRUniqueDeleterStorage<TDeleter>::getDeleter();
	}
	const TDeleter& getDeleter( void ) const noexcept
	{
		return SIRUniqueDeleterStorage<TDeleter>::getDeleter();
	}
	T *release( void ) noexcept
	{
		T *pObject = m_pObject;
		m_pObject = nullptr;
		return pObject;
	}
	void reset( T *pObject = nullptr ) noexcept
	{
		if ( m_pObject != nullptr && m_pObject != pObject ) {
			getDeleter()( m_pObject );
		}
		m_pObject = pObject;
	}

private:
	T *m_pObject = nullptr;
};

#endif // __SIRENGINE_SIRUNIQUEPTR_H__

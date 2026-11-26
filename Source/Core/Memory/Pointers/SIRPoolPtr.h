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

#ifndef __SIR_POOLPTR_H__
#define __SIR_POOLPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRPoolAllocator.h>

#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// Exclusive ownership of an object in a fixed-size SIRPoolAllocator slot.
// The pool must outlive the pointer, and its mutation is single-owner only.
// Construction failure returns the slot to the pool when exceptions are enabled.
template <typename T>
class SIRPoolPtr
{
public:
	SIRPoolPtr( void ) noexcept = default;
	SIRPoolPtr( const SIRPoolPtr& ) = delete;
	SIRPoolPtr( SIRPoolPtr&& rOther ) noexcept
		: m_pPool( rOther.m_pPool ), m_pObject( rOther.m_pObject )
	{
		rOther.m_pPool = nullptr;
		rOther.m_pObject = nullptr;
	}
	~SIRPoolPtr( void ) noexcept
	{
		reset();
	}

	SIRPoolPtr& operator=( const SIRPoolPtr& ) = delete;
	SIRPoolPtr& operator=( SIRPoolPtr&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pPool = rOther.m_pPool;
			m_pObject = rOther.m_pObject;
			rOther.m_pPool = nullptr;
			rOther.m_pObject = nullptr;
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
	explicit operator bool( void ) const noexcept
	{
		return m_pObject != nullptr;
	}

	void reset( void ) noexcept
	{
		T *pObject = m_pObject;
		SIRPoolAllocator *pPool = m_pPool;
		m_pObject = nullptr;
		m_pPool = nullptr;
		if ( pObject != nullptr ) {
			pObject->~T();
			if ( !pPool->free( pObject ) ) {
				abort();
			}
		}
	}

	template <typename... TArgs>
	static SIRPoolPtr create( SIRPoolAllocator *pPool, TArgs&&...args )
	{
		static_assert( std::is_nothrow_destructible<T>::value, "Pool-owned destructors must not throw" );
		SIRPoolPtr result;
		if ( pPool == nullptr || pPool->getObjectBytes() < sizeof( T ) ) {
			return result;
		}

		void *pMemory = pPool->alloc();
		if ( pMemory == nullptr ) {
			return result;
		}
		if ( ( reinterpret_cast<uintptr_t>(pMemory) % alignof( T ) ) != 0 ) {
			static_cast<void>(pPool->free( pMemory ));
			return result;
		}

#if defined( __cpp_exceptions )
		try {
			result.m_pObject = ::new ( pMemory ) T( std::forward<TArgs>( args )... );
		} catch ( ... ) {
			static_cast<void>(pPool->free( pMemory ));
			throw;
		}
#else
		static_assert( std::is_nothrow_constructible<T, TArgs...>::value, "Pool construction must be noexcept without exceptions" );
		result.m_pObject = ::new ( pMemory ) T( std::forward<TArgs>( args )... );
#endif
		result.m_pPool = pPool;
		return result;
	}

private:
	SIRPoolAllocator *m_pPool = nullptr;
	T *m_pObject = nullptr;
};

#endif // __SIR_POOLPTR_H__

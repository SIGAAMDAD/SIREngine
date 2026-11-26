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

#ifndef __SIR_ALLOCATIONHANDLE_H__
#define __SIR_ALLOCATIONHANDLE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRHandleHeap.h>

#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// A pin prevents the existing SIRHandleHeap from relocating its allocation.
// The heap must outlive every pin and handle, and its mutations are single-owner.
template <typename T>
class SIRAllocationPin
{
public:
	SIRAllocationPin( void ) noexcept = default;
	SIRAllocationPin( const SIRAllocationPin& ) = delete;
	SIRAllocationPin( SIRAllocationPin&& rOther ) noexcept
	    : m_pHeap( rOther.m_pHeap ), m_Handle( rOther.m_Handle ), m_pObject( rOther.m_pObject )
	{
		rOther.m_pHeap = nullptr;
		rOther.m_pObject = nullptr;
	}
	~SIRAllocationPin( void ) noexcept
	{
		reset();
	}

	SIRAllocationPin& operator=( const SIRAllocationPin& ) = delete;
	SIRAllocationPin& operator=( SIRAllocationPin&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			reset();
			m_pHeap = rOther.m_pHeap;
			m_Handle = rOther.m_Handle;
			m_pObject = rOther.m_pObject;
			rOther.m_pHeap = nullptr;
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
		if ( m_pHeap != nullptr ) {
			if ( !m_pHeap->unpin( m_Handle ) ) {
				abort();
			}
		}
		m_pHeap = nullptr;
		m_pObject = nullptr;
	}

private:
	template <typename U>
	friend class SIRAllocationHandle;
	SIRAllocationPin( SIRHandleHeap *pHeap, sirMemoryHandle_t handle, T *pObject ) noexcept
	    : m_pHeap( pHeap ), m_Handle( handle ), m_pObject( pObject )
	{
	}

	SIRHandleHeap *m_pHeap = nullptr;
	sirMemoryHandle_t m_Handle {};
	T *m_pObject = nullptr;
};

// Typed non-owning generational handle to SIRHandleHeap's relocatable bytes.
// The allocator may compact allocations only when no pin is active. Only
// trivially copyable types are supported because compaction memmoves bytes.
template <typename T>
class SIRAllocationHandle
{
public:
	SIRAllocationHandle( void ) noexcept = default;

	bool isValid( void ) const noexcept
	{
		return m_pHeap != nullptr && m_pHeap->getLifetimeGeneration() == m_nHeapGeneration && m_pHeap->resolve( m_Handle ) != nullptr;
	}
	SIRAllocationPin<T> pin( void ) const noexcept
	{
		if ( m_pHeap == nullptr || m_pHeap->getLifetimeGeneration() != m_nHeapGeneration ) {
			return {};
		}
		void *pObject = m_pHeap->pin( m_Handle );
		if ( pObject == nullptr ) {
			return {};
		}
		return SIRAllocationPin<T>( m_pHeap, m_Handle, static_cast<T *>(pObject) );
	}
	bool release( void ) noexcept
	{
		if ( m_pHeap == nullptr || m_pHeap->getLifetimeGeneration() != m_nHeapGeneration || !m_pHeap->free( m_Handle ) ) {
			return false;
		}
		m_pHeap = nullptr;
		return true;
	}
	sirMemoryHandle_t getRawHandle( void ) const noexcept
	{
		return m_Handle;
	}

	template <typename... TArgs>
	static SIRAllocationHandle allocate( SIRHandleHeap *pHeap, TArgs&&...args ) noexcept
	{
		static_assert( std::is_trivially_copyable<T>::value, "HandleHeap compacts objects with raw memmove" );
		static_assert( std::is_trivially_destructible<T>::value, "HandleHeap does not call destructors" );
		static_assert( std::is_nothrow_constructible<T, TArgs...>::value, "Handle allocation construction must not throw" );
		SIRAllocationHandle result;
		if ( pHeap == nullptr ) {
			return result;
		}

		sirMemoryHandle_t handle {};
		void *pMemory = pHeap->alloc( sizeof( T ), alignof( T ), &handle );
		if ( pMemory == nullptr ) {
			return result;
		}
		::new ( pMemory ) T( std::forward<TArgs>( args )... );
		result.m_pHeap = pHeap;
		result.m_nHeapGeneration = pHeap->getLifetimeGeneration();
		result.m_Handle = handle;
		return result;
	}

private:
	SIRHandleHeap *m_pHeap = nullptr;
	sirMemoryHandle_t m_Handle {};
	uint64_t m_nHeapGeneration = 0;
};

#endif // __SIR_ALLOCATIONHANDLE_H__

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

#ifndef __SIR_ARENAPTR_H__
#define __SIR_ARENAPTR_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRLinearAllocator.h>

#include <new>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// Borrowed linear-arena object; rewind/reset/shutdown invalidate earlier
// pointers conservatively, even if the rewound bytes are subsequently reused.
// Linear allocator ownership and mutation remain single-threaded.
template <typename T>
class SIRArenaPtr
{
public:
	SIRArenaPtr( void ) noexcept = default;

	T *get( void ) const noexcept
	{
		return isValid() ? m_pObject : nullptr;
	}
	T *operator->( void ) const noexcept
	{
		T *pObject = get();
		if ( pObject == nullptr ) {
			abort();
		}
		return pObject;
	}
	T& operator*( void ) const noexcept
	{
		return *operator->();
	}
	explicit operator bool( void ) const noexcept
	{
		return isValid();
	}
	bool isValid( void ) const noexcept
	{
		return m_pArena != nullptr && m_pObject != nullptr && m_pArena->getLifetimeGeneration() == m_nGeneration;
	}

	template <typename... TArgs>
	static SIRArenaPtr create( SIRLinearAllocator *pArena, TArgs&&...args ) noexcept
	{
		static_assert( std::is_trivially_destructible<T>::value, "Arena reset cannot run individual destructors" );
		static_assert( std::is_nothrow_constructible<T, TArgs...>::value, "Arena constructors must not throw" );
		SIRArenaPtr result;
		if ( pArena == nullptr || pArena->getSegment() == nullptr ) {
			return result;
		}

		void *pMemory = pArena->alloc( sizeof( T ), alignof( T ) );
		if ( pMemory == nullptr ) {
			return result;
		}
		result.m_pObject = ::new ( pMemory ) T( std::forward<TArgs>( args )... );
		result.m_pArena = pArena;
		result.m_nGeneration = pArena->getLifetimeGeneration();
		return result;
	}

private:
	SIRLinearAllocator *m_pArena = nullptr;
	T *m_pObject = nullptr;
	uint64_t m_nGeneration = 0;
};

#endif // __SIR_ARENAPTR_H__

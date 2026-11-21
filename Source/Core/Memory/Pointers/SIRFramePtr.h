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

#ifndef __SIRENGINE_SIRFRAMEPTR_H__
#define __SIRENGINE_SIRFRAMEPTR_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Allocators/SIRFrameAllocator.h>

#include <new>
#include <stdlib.h>
#include <type_traits>
#include <utility>

// Borrowed frame allocation: no per-object free and no destructor call.
// Only trivially destructible objects are permitted; submitted epochs are
// inaccessible from the CPU, even while their GPU work remains in flight.
template <typename T>
class SIRFramePtr
{
public:
	SIRFramePtr( void ) noexcept = default;

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
		return m_pFrame != nullptr && m_pFrame->getLifetimeGeneration() == m_nGeneration && m_pFrame->getCurrentEpoch() == m_nEpoch;
	}
	uint64_t getEpoch( void ) const noexcept
	{
		return m_nEpoch;
	}

	template <typename... TArgs>
	static SIRFramePtr create( SIRFrameAllocator *pFrame, TArgs&&...args ) noexcept
	{
		static_assert( std::is_trivially_destructible<T>::value, "Frame allocations do not run object destructors" );
		static_assert( std::is_nothrow_constructible<T, TArgs...>::value, "Frame constructors must not throw" );
		SIRFramePtr result;
		if ( pFrame == nullptr ) {
			return result;
		}
		const uint64_t nEpoch = pFrame->getCurrentEpoch();
		if ( nEpoch == 0 ) {
			return result;
		}

		void *pMemory = pFrame->alloc( sizeof( T ), alignof( T ) );
		if ( pMemory == nullptr ) {
			return result;
		}
		result.m_pObject = ::new ( pMemory ) T( std::forward<TArgs>( args )... );
		result.m_pFrame = pFrame;
		result.m_nEpoch = nEpoch;
		result.m_nGeneration = pFrame->getLifetimeGeneration();
		return result;
	}

private:
	SIRFrameAllocator *m_pFrame = nullptr;
	T *m_pObject = nullptr;
	uint64_t m_nEpoch = 0;
	uint64_t m_nGeneration = 0;
};

#endif // __SIRENGINE_SIRFRAMEPTR_H__

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

#ifndef __SIRENGINE_SIRRAWSTORAGE_H__
#define __SIRENGINE_SIRRAWSTORAGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <limits.h>
#include <new>
#include <type_traits>
#include <utility>

// All allocations use an explicit, caller-owned domain descriptor.
class SIRRawStorage
{
public:
	static bool isValidAllocator( const sirContainerAllocator_t *pAllocator ) noexcept
	{
		return pAllocator != nullptr && pAllocator->pfnAllocate != nullptr && pAllocator->pfnDeallocate != nullptr;
	}

	static bool checkedMultiply( size_t nLeft, size_t nRight, size_t *pProduct ) noexcept
	{
		if ( pProduct == nullptr || ( nRight != 0 && nLeft > SIZE_MAX / nRight ) ) {
			return false;
		}
		*pProduct = nLeft * nRight;
		return true;
	}

	static bool checkedAdd( size_t nLeft, size_t nRight, size_t *pSum ) noexcept
	{
		if ( pSum == nullptr || nLeft > SIZE_MAX - nRight ) {
			return false;
		}
		*pSum = nLeft + nRight;
		return true;
	}

	static sirContainerResult_t tryAllocate( const sirContainerAllocator_t *pAllocator, size_t nBytes, size_t nAlignment,
	                                         void **ppMemory ) noexcept
	{
		if ( ppMemory == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		*ppMemory = nullptr;
		if ( nBytes == 0 ) {
			return SIR_CONTAINER_OK;
		}
		if ( !isValidAllocator( pAllocator ) || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		void *pMemory = pAllocator->pfnAllocate( pAllocator->pContext, nBytes, nAlignment );
		if ( pMemory == nullptr ) {
			return SIR_CONTAINER_OUT_OF_MEMORY;
		}
		if ( ( ( uintptr_t )pMemory & ( nAlignment - 1 ) ) != 0 ) {
			pAllocator->pfnDeallocate( pAllocator->pContext, pMemory, nBytes, nAlignment );
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		*ppMemory = pMemory;
		return SIR_CONTAINER_OK;
	}

	static void deallocate( const sirContainerAllocator_t *pAllocator, void *pMemory, size_t nBytes, size_t nAlignment ) noexcept
	{
		if ( pMemory != nullptr ) {
			pAllocator->pfnDeallocate( pAllocator->pContext, pMemory, nBytes, nAlignment );
		}
	}
};

// The relocation path provides the strong failure guarantee for reserve.
template <typename T>
class SIRLifetimeOps
{
public:
	static constexpr bool CAN_RELOCATE = std::is_nothrow_move_constructible<T>::value || std::is_copy_constructible<T>::value;

	static void destroy( T *pData, uint32_t nCount ) noexcept
	{
		for ( uint32_t iElement = nCount; iElement > 0; --iElement ) {
			pData[iElement - 1].~T();
		}
	}

	static void relocate( T *pDestination, T *pSource, uint32_t nCount )
	{
		static_assert( CAN_RELOCATE, "Transactional relocation requires nothrow move construction or copy construction" );
		uint32_t nConstructed = 0;
		try {
			for ( ; nConstructed < nCount; ++nConstructed ) {
				if constexpr ( std::is_nothrow_move_constructible<T>::value ) {
					new ( pDestination + nConstructed ) T( std::move( pSource[nConstructed] ) );
				} else {
					new ( pDestination + nConstructed ) T( pSource[nConstructed] );
				}
			}
		} catch ( ... ) {
			destroy( pDestination, nConstructed );
			throw;
		}
	}
};

#endif // __SIRENGINE_SIRRAWSTORAGE_H__

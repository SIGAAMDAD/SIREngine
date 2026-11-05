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

#ifndef __SIRENGINE_SIRCONTAINERHEAPADAPTER_H__
#define __SIRENGINE_SIRCONTAINERHEAPADAPTER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <Core/Memory/SIRGeneralHeap.h>
#include <assert.h>

// Binds arbitrary-lifetime Core containers to a single tracked manager domain.
// The heap and adapter must outlive every container bound to getAllocator().
// Initialize once before concurrent use. No system-heap fallback or hidden
// allocations. A domain requires a heap whose three backends own that domain.
class SIRContainerHeapAdapter
{
private:
	SIRGeneralHeap *m_pHeap = nullptr;
	sirContainerAllocator_t m_Allocator = {};

	static void *allocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
	{
		SIRContainerHeapAdapter *pAdapter = ( SIRContainerHeapAdapter * )pContext;
		if ( pAdapter == nullptr || pAdapter->m_pHeap == nullptr ) {
			return nullptr;
		}
		return pAdapter->m_pHeap->allocAligned( nBytes, nAlignment );
	}

	static void deallocate( void *pContext, void *pMemory, size_t, size_t ) noexcept
	{
		SIRContainerHeapAdapter *pAdapter = ( SIRContainerHeapAdapter * )pContext;
		assert( pAdapter != nullptr && pAdapter->m_pHeap != nullptr );
		if ( pAdapter != nullptr && pAdapter->m_pHeap != nullptr ) {
			const bool bReleased = pAdapter->m_pHeap->tryFree( pMemory );
			assert( bReleased );
			( void )bReleased;
		}
	}

public:
	SIRContainerHeapAdapter( void ) noexcept = default;
	SIRContainerHeapAdapter( const SIRContainerHeapAdapter& ) = delete;
	SIRContainerHeapAdapter& operator=( const SIRContainerHeapAdapter& ) = delete;
	SIRContainerHeapAdapter( SIRContainerHeapAdapter&& ) = delete;
	SIRContainerHeapAdapter& operator=( SIRContainerHeapAdapter&& ) = delete;

	bool init( SIRGeneralHeap *pHeap, sirMemoryDomain_t eDomain, uint32_t nOwnerTag ) noexcept
	{
		if ( m_pHeap != nullptr || pHeap == nullptr || eDomain >= SIR_MEMORY_DOMAIN_COUNT || !pHeap->isConfiguredForDomain( eDomain ) ) {
			return false;
		}
		m_pHeap = pHeap;
		m_Allocator.pContext = this;
		m_Allocator.pfnAllocate = allocate;
		m_Allocator.pfnDeallocate = deallocate;
		m_Allocator.nDomainId = ( uint32_t )eDomain;
		m_Allocator.nOwnerTag = nOwnerTag;
		return true;
	}

	const sirContainerAllocator_t *getAllocator( void ) const noexcept
	{
		return m_pHeap != nullptr ? &m_Allocator : nullptr;
	}
};

#endif // __SIRENGINE_SIRCONTAINERHEAPADAPTER_H__

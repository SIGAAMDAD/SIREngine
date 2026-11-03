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

#include <Resources/SIRResourceMemoryAdapter.h>

#include <Core/Memory/SIRGeneralHeap.h>
#include <stdint.h>
#include <stdlib.h>

SIRResourceMemoryAdapter::SIRResourceMemoryAdapter( void ) noexcept : m_pHeaps{}
{
}

bool SIRResourceMemoryAdapter::bindDomain( sirMemoryDomain_t eDomain, SIRGeneralHeap *pHeap ) noexcept
{
	if ( eDomain >= SIR_MEMORY_DOMAIN_COUNT || pHeap == nullptr || m_pHeaps[eDomain] != nullptr ||
	     !pHeap->isConfiguredForDomain( eDomain ) ) {
		return false;
	}
	m_pHeaps[eDomain] = pHeap;
	return true;
}

SIRResourceAllocator SIRResourceMemoryAdapter::getAllocator( void ) noexcept
{
	return { this, allocate, release };
}

void *SIRResourceMemoryAdapter::allocate( void *pContext, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	if ( pContext == nullptr || nBytes == 0 || nBytes > SIZE_MAX || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 ||
	     nDomain >= SIR_MEMORY_DOMAIN_COUNT ) {
		return nullptr;
	}
	SIRResourceMemoryAdapter *pAdapter = ( SIRResourceMemoryAdapter * )pContext;
	SIRGeneralHeap *pHeap = pAdapter->m_pHeaps[nDomain];
	return pHeap != nullptr ? pHeap->allocAligned( ( size_t )nBytes, ( size_t )nAlignment ) : nullptr;
}

void SIRResourceMemoryAdapter::release( void *pContext, void *pData, uint64_t nBytes, uint32_t nAlignment, uint32_t nDomain ) noexcept
{
	( void )nBytes;
	( void )nAlignment;
	if ( pData == nullptr ) {
		return;
	}
	if ( pContext == nullptr || nDomain >= SIR_MEMORY_DOMAIN_COUNT ) {
		abort();
	}
	SIRResourceMemoryAdapter *pAdapter = ( SIRResourceMemoryAdapter * )pContext;
	SIRGeneralHeap *pHeap = pAdapter->m_pHeaps[nDomain];
	if ( pHeap == nullptr || !pHeap->owns( pData ) || !pHeap->tryFree( pData ) ) {
		abort();
	}
}

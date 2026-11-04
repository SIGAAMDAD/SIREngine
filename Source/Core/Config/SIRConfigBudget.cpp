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

#include <Core/Config/SIRConfigBudget.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>

// A header immediately precedes the aligned user region. The original callback
// receives exactly the base address, size and alignment it returned.
typedef struct sirConfigAllocationHeader_s
{
	SIRConfigBudget *pOwner;
	void *pBase;
	size_t nTotalBytes;
	size_t nBackingAlignment;
	size_t nUserBytes;
	size_t nUserAlignment;
} sirConfigAllocationHeader_t;

SIRConfigBudget::SIRConfigBudget( void ) noexcept
    : m_pBacking( nullptr ), m_Allocator{}, m_nLimit( 0 ), m_nLive( 0 ), m_nPeak( 0 ), m_nAllocations( 0 ), m_nRejected( 0 )
{
}

SIRConfigBudget::~SIRConfigBudget( void ) noexcept
{
	assert( m_nLive == 0 );
}

bool SIRConfigBudget::init( const sirContainerAllocator_t *pBacking, uint64_t nHardLimitBytes ) noexcept
{
	if ( m_pBacking != nullptr || pBacking == nullptr || pBacking->pfnAllocate == nullptr || pBacking->pfnDeallocate == nullptr ||
	     nHardLimitBytes == 0 ) {
		return false;
	}

	m_pBacking = pBacking;
	m_nLimit = nHardLimitBytes;
	m_Allocator.pContext = this;
	m_Allocator.pfnAllocate = allocate;
	m_Allocator.pfnDeallocate = deallocate;
	m_Allocator.nDomainId = pBacking->nDomainId;
	m_Allocator.nOwnerTag = pBacking->nOwnerTag;
	return true;
}

bool SIRConfigBudget::shutdown( void ) noexcept
{
	if ( m_nLive != 0 ) {
		return false;
	}

	m_pBacking = nullptr;
	m_Allocator = {};
	m_nLimit = 0;
	return true;
}

const sirContainerAllocator_t *SIRConfigBudget::getAllocator( void ) const noexcept
{
	return m_pBacking != nullptr ? &m_Allocator : nullptr;
}

uint64_t SIRConfigBudget::getLiveBytes( void ) const noexcept
{
	return m_nLive;
}

uint64_t SIRConfigBudget::getPeakBytes( void ) const noexcept
{
	return m_nPeak;
}

uint64_t SIRConfigBudget::getAllocations( void ) const noexcept
{
	return m_nAllocations;
}

uint64_t SIRConfigBudget::getRejectedAllocations( void ) const noexcept
{
	return m_nRejected;
}

void *SIRConfigBudget::allocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	SIRConfigBudget *pBudget = ( SIRConfigBudget * )pContext;
	if ( pBudget == nullptr || pBudget->m_pBacking == nullptr ) {
		return nullptr;
	}

	const bool bValidAlignment = nAlignment != 0 && ( nAlignment & ( nAlignment - 1 ) ) == 0;
	if ( !bValidAlignment || nBytes == 0 || nBytes > pBudget->m_nLimit - pBudget->m_nLive ) {
		++pBudget->m_nRejected;
		return nullptr;
	}

	const size_t nEffectiveAlignment =
	    nAlignment < alignof( sirConfigAllocationHeader_t ) ? alignof( sirConfigAllocationHeader_t ) : nAlignment;
	const size_t nOverhead = sizeof( sirConfigAllocationHeader_t ) + nEffectiveAlignment - 1;
	if ( nOverhead < nEffectiveAlignment || nBytes > SIZE_MAX - nOverhead ) {
		++pBudget->m_nRejected;
		return nullptr;
	}

	const size_t nTotal = nBytes + nOverhead;
	void *pBase = pBudget->m_pBacking->pfnAllocate( pBudget->m_pBacking->pContext, nTotal, nEffectiveAlignment );
	if ( pBase == nullptr ) {
		++pBudget->m_nRejected;
		return nullptr;
	}

	const uintptr_t nStart = ( uintptr_t )pBase + sizeof( sirConfigAllocationHeader_t );
	const uintptr_t nAligned = ( nStart + nEffectiveAlignment - 1 ) & ~( ( uintptr_t )nEffectiveAlignment - 1 );
	sirConfigAllocationHeader_t *pHeader = ( sirConfigAllocationHeader_t * )( nAligned - sizeof( sirConfigAllocationHeader_t ) );
	pHeader->pOwner = pBudget;
	pHeader->pBase = pBase;
	pHeader->nTotalBytes = nTotal;
	pHeader->nBackingAlignment = nEffectiveAlignment;
	pHeader->nUserBytes = nBytes;
	pHeader->nUserAlignment = nAlignment;
	pBudget->m_nLive += nBytes;
	++pBudget->m_nAllocations;
	if ( pBudget->m_nLive > pBudget->m_nPeak ) {
		pBudget->m_nPeak = pBudget->m_nLive;
	}

	return ( void * )nAligned;
}

void SIRConfigBudget::deallocate( void *pContext, void *pMemory, size_t nBytes, size_t nAlignment ) noexcept
{
	if ( pMemory == nullptr ) {
		return;
	}

	SIRConfigBudget *pBudget = ( SIRConfigBudget * )pContext;
	if ( pBudget == nullptr || pBudget->m_pBacking == nullptr ) {
		assert( false );
		return;
	}

	sirConfigAllocationHeader_t *pHeader =
	    ( sirConfigAllocationHeader_t * )( ( uint8_t * )pMemory - sizeof( sirConfigAllocationHeader_t ) );
	if ( pHeader->pOwner != pBudget || pHeader->nUserBytes != nBytes || pHeader->nUserAlignment != nAlignment ) {
		assert( false );
		return;
	}

	assert( pBudget->m_nLive >= pHeader->nUserBytes );
	pBudget->m_nLive -= pHeader->nUserBytes;
	pBudget->m_pBacking->pfnDeallocate( pBudget->m_pBacking->pContext, pHeader->pBase, pHeader->nTotalBytes, pHeader->nBackingAlignment );
}

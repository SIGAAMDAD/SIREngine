/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRSync.h>
#include <Core/Memory/SIRGeneralHeap.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRPageRunAllocator.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <cstring>
#include <limits>

static thread_local uint32_t s_nSmallAllocServiceCount = 0;

SIRGeneralHeap::SIRGeneralHeap( void ) : m_Config(), m_bInitialized( false )
{
}

bool SIRGeneralHeap::contains( const SIRMemorySegment *pSegment, const void *pMemory )
{
	if ( pSegment == nullptr || pMemory == nullptr ) {
		return false;
	}
	const uintptr_t nBase = ( ( uintptr_t )( pSegment->getBase() ) );
	const uintptr_t nPointer = ( ( uintptr_t )( pMemory ) );
	return nPointer >= nBase && nPointer - nBase < pSegment->getReservedBytes();
}

bool SIRGeneralHeap::overlap( const SIRMemorySegment *pA, const SIRMemorySegment *pB )
{
	if ( pA == nullptr || pB == nullptr ) {
		return false;
	}
	const uintptr_t nA = ( ( uintptr_t )( pA->getBase() ) );
	const uintptr_t nB = ( ( uintptr_t )( pB->getBase() ) );
	return ( nA <= nB && nB - nA < pA->getReservedBytes() ) || ( nB <= nA && nA - nB < pB->getReservedBytes() );
}

bool SIRGeneralHeap::init( const sirGeneralHeapConfig_t& config )
{
	if ( m_bInitialized || config.pSmall == nullptr || config.pSmallSegment == nullptr || config.pMediumSegment == nullptr ||
	     config.pLargeSegment == nullptr || ( config.bUsePageRuns ? config.pPageRuns == nullptr : config.pLarge == nullptr ) ||
	     config.nSmallMaximumBytes == 0 || config.nSmallMaximumBytes > config.pSmall->getMaximumBytes() ||
	     config.nLargeMinimumBytes <= config.nSmallMaximumBytes ||
	     ( config.bUseTLSF ? config.pTLSF == nullptr : config.pFreeList == nullptr ) ||
	     overlap( config.pSmallSegment, config.pMediumSegment ) || overlap( config.pSmallSegment, config.pLargeSegment ) ||
	     overlap( config.pMediumSegment, config.pLargeSegment ) ||
	     config.pSmallSegment->getManager() != config.pMediumSegment->getManager() ||
	     config.pSmallSegment->getManager() != config.pLargeSegment->getManager() ||
	     !config.pSmall->owns( config.pSmallSegment->getBase() ) ) {
		return false;
	}
	m_Config = config;
	m_bInitialized = true;
	return true;
}

bool SIRGeneralHeap::isConfiguredForDomain( sirMemoryDomain_t eDomain ) const noexcept
{
	return m_bInitialized && eDomain < SIR_MEMORY_DOMAIN_COUNT && m_Config.pSmallSegment->getDomain() == eDomain &&
	       m_Config.pMediumSegment->getDomain() == eDomain && m_Config.pLargeSegment->getDomain() == eDomain;
}

bool SIRGeneralHeap::attachThread( void )
{
	return m_bInitialized && m_Config.pSmall->attach();
}

bool SIRGeneralHeap::detachThread( void )
{
	return m_bInitialized && m_Config.pSmall->detach();
}

sirHeapRoute_t SIRGeneralHeap::routePointer( const void *pMemory ) const
{
	if ( !m_bInitialized ) {
		return SIR_HEAP_NONE;
	}
	if ( contains( m_Config.pSmallSegment, pMemory ) ) {
		return SIR_HEAP_SMALL;
	}
	if ( contains( m_Config.pMediumSegment, pMemory ) ) {
		return SIR_HEAP_MEDIUM;
	}
	if ( contains( m_Config.pLargeSegment, pMemory ) ) {
		return SIR_HEAP_LARGE;
	}
	return SIR_HEAP_NONE;
}

void *SIRGeneralHeap::allocRaw( size_t nBytes, size_t nAlignment )
{
	if ( !m_bInitialized || nBytes == 0 || nAlignment == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) ) {
		return nullptr;
	}
	if ( nBytes <= m_Config.nSmallMaximumBytes && nAlignment <= m_Config.pSmall->getMinimumAlignment() ) {
		if ( m_Config.pSmall->getAttachedSeat() != UINT32_MAX || m_Config.pSmall->attach() ) {
			// Bounded owner-side reclamation; no global atomic on the ordinary
			// allocation path. A refill also services the pending inbox.
			if ( ++s_nSmallAllocServiceCount == 64 ) {
				s_nSmallAllocServiceCount = 0;
				m_Config.pSmall->drainRemote();
				if ( m_Config.pSmall->hasPendingOrphans() ) {
					m_Config.pSmall->serviceOrphans( 128 );
				}
			}
			void *pMemory = m_Config.pSmall->alloc( ( ( uint32_t )( nBytes ) ), ( ( uint32_t )( nAlignment ) ) );
			if ( pMemory == nullptr ) {
				m_Config.pSmall->drainRemote();
				pMemory = m_Config.pSmall->alloc( ( ( uint32_t )( nBytes ) ), ( ( uint32_t )( nAlignment ) ) );
			}
			return pMemory;
		}
		// Transient engine threads can outnumber the preprovisioned seats.
		// Route them into the synchronized medium heap, without CRT fallback.
	}
	if ( nBytes < m_Config.nLargeMinimumBytes ) {
		if ( nBytes > UINT32_MAX || nAlignment > UINT32_MAX ) {
			return nullptr;
		}
		SIRScopedLock<SIRMutex> lock( m_MediumMutex );
		return m_Config.bUseTLSF ? m_Config.pTLSF->alloc( ( ( uint32_t )( nBytes ) ), ( ( uint32_t )( nAlignment ) ) )
		                         : m_Config.pFreeList->alloc( ( ( uint32_t )( nBytes ) ), ( ( uint32_t )( nAlignment ) ) );
	}
	SIRScopedLock<SIRMutex> lock( m_LargeMutex );
	return m_Config.bUsePageRuns ? m_Config.pPageRuns->alloc( ( ( uint64_t )( nBytes ) ), ( ( uint64_t )( nAlignment ) ) )
	                             : m_Config.pLarge->alloc( ( ( uint64_t )( nBytes ) ), ( ( uint64_t )( nAlignment ) ) );
}

void *SIRGeneralHeap::alloc( size_t nBytes )
{
	return allocRaw( nBytes, 16 );
}

void *SIRGeneralHeap::allocAligned( size_t nBytes, size_t nAlignment )
{
	return allocRaw( nBytes, nAlignment );
}

void *SIRGeneralHeap::allocZeroed( size_t nCount, size_t nBytes )
{
	if ( nBytes != 0 && nCount > SIZE_MAX / nBytes ) {
		return nullptr;
	}
	const size_t nTotal = nCount * nBytes;
	void *pMemory = alloc( nTotal );
	if ( pMemory != nullptr ) {
		std::memset( pMemory, 0, nTotal );
	}
	return pMemory;
}

size_t SIRGeneralHeap::getUsableSize( const void *pMemory ) const
{
	switch ( routePointer( pMemory ) ) {
	case SIR_HEAP_SMALL:
		return m_Config.pSmall->getUsableSize( pMemory );
	case SIR_HEAP_MEDIUM: {
		SIRScopedLock<SIRMutex> lock( m_MediumMutex );
		return m_Config.bUseTLSF ? m_Config.pTLSF->getUsableSize( pMemory ) : m_Config.pFreeList->getUsableSize( pMemory );
	}
	case SIR_HEAP_LARGE: {
		SIRScopedLock<SIRMutex> lock( m_LargeMutex );
		return ( ( size_t )( m_Config.bUsePageRuns ? m_Config.pPageRuns->getUsableSize( pMemory )
		                                           : m_Config.pLarge->getUsableSize( pMemory ) ) );
	}
	default:
		return 0;
	}
}

bool SIRGeneralHeap::owns( const void *pMemory ) const
{
	return routePointer( pMemory ) != SIR_HEAP_NONE;
}

bool SIRGeneralHeap::tryFree( void *pMemory )
{
	if ( pMemory == nullptr ) {
		return true;
	}
	switch ( routePointer( pMemory ) ) {
	case SIR_HEAP_SMALL: {
		const bool bFreed = m_Config.pSmall->free( pMemory );
		if ( bFreed ) {
			if ( m_Config.pSmall->hasPendingOrphans() ) {
				m_Config.pSmall->serviceOrphans( 128 );
			}
		}
		return bFreed;
	}
	case SIR_HEAP_MEDIUM: {
		SIRScopedLock<SIRMutex> lock( m_MediumMutex );
		return m_Config.bUseTLSF ? m_Config.pTLSF->free( pMemory ) : m_Config.pFreeList->free( pMemory );
	}
	case SIR_HEAP_LARGE: {
		SIRScopedLock<SIRMutex> lock( m_LargeMutex );
		return m_Config.bUsePageRuns ? m_Config.pPageRuns->free( pMemory ) : m_Config.pLarge->free( pMemory );
	}
	default:
		return false;
	}
}

void SIRGeneralHeap::free( void *pMemory )
{
	( void )tryFree( pMemory );
}

void *SIRGeneralHeap::realloc( void *pMemory, size_t nNewBytes )
{
	return reallocAligned( pMemory, nNewBytes, 16 );
}

void *SIRGeneralHeap::reallocAligned( void *pMemory, size_t nNewBytes, size_t nAlignment )
{
	if ( pMemory == nullptr ) {
		return allocAligned( nNewBytes, nAlignment );
	}
	if ( nNewBytes == 0 ) {
		free( pMemory );
		return nullptr;
	}
	const size_t nOldCapacity = getUsableSize( pMemory );
	if ( nOldCapacity == 0 ) {
		return nullptr;
	}
	if ( !SIR_IS_POWER_OF_TWO( nAlignment ) ) {
		return nullptr;
	}
	if ( nNewBytes <= nOldCapacity && ( ( ( uintptr_t )( pMemory ) ) % nAlignment ) == 0 ) {
		return pMemory;
	}
	void *pNew = allocAligned( nNewBytes, nAlignment );
	if ( pNew == nullptr ) {
		return nullptr;
	}
	std::memcpy( pNew, pMemory, nOldCapacity < nNewBytes ? nOldCapacity : nNewBytes );
	if ( !tryFree( pMemory ) ) {
		tryFree( pNew );
		return nullptr;
	}
	return pNew;
}

uint32_t SIRGeneralHeap::serviceThread( uint32_t nMaximum )
{
	if ( !m_bInitialized || nMaximum == 0 ) {
		return 0;
	}
	return m_Config.pSmall->servicePending( nMaximum );
}

uint32_t SIRGeneralHeap::drainRemote( void )
{
	return m_bInitialized ? m_Config.pSmall->drainRemote() : 0;
}

bool SIRGeneralHeap::shutdown( void )
{
	if ( !m_bInitialized ) {
		return true;
	}
	// Backend lifetimes are external; live objects must be freed before teardown.
	if ( m_Config.pSmall->getLiveCount() != 0 ) {
		return false;
	}
	{
		SIRScopedLock<SIRMutex> lock( m_MediumMutex );
		if ( m_Config.bUseTLSF ? m_Config.pTLSF->getAllocationCount() != 0 : m_Config.pFreeList->getAllocationCount() != 0 ) {
			return false;
		}
	}
	{
		SIRScopedLock<SIRMutex> lock( m_LargeMutex );
		if ( ( m_Config.bUsePageRuns ? m_Config.pPageRuns->getAllocationCount() : m_Config.pLarge->getAllocationCount() ) != 0 ) {
			return false;
		}
	}
	m_bInitialized = false;
	return true;
}

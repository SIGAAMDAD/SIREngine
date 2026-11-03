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

#include <Core/Platform/SIRAtomic.h>
#include <Core/Platform/SIRScopedLock.h>
#include <Core/Memory/SIRMemoryPressureCoordinator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/Allocators/SIRSlabAllocator.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRThreadLocalAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRCacheAllocator.h>
#include <Core/Memory/Allocators/SIRHandleHeap.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRPoolAllocator.h>
#include <Core/Memory/Allocators/SIRFrameAllocator.h>
#include <Core/Memory/Allocators/SIRRingAllocator.h>
#include <new>

sirOwner_s::sirOwner_s( void )
    : pAllocator( nullptr ), ppSegments( nullptr ), nSegmentCount( 0 ),
      nType( 0 ), nSeat( 0 ), nWarmSlabs( 0 ), nMaximumVictims( 0 ), nPriority( 0 ),
      nPendingBytes( 0 ), nServicing( 0 )
{
}

SIRMemoryPressureCoordinator::SIRMemoryPressureCoordinator( void )
    : m_pManager( nullptr ), m_pOwners( nullptr ), m_nOwnerCapacity( 0 ),
      m_nOwnerCount( 0 ), m_nRequestedBytes( 0 ), m_nReclaimedBytes( 0 ),
      m_nCanceledBytes( 0 )
{
}

bool SIRMemoryPressureCoordinator::init( SIRMemoryManager *pManager,
                                         uint32_t nOwnerCapacity )
{
    if ( m_pManager != nullptr || pManager == nullptr || nOwnerCapacity == 0 ||
         !pManager->isInitialized() )
    {
        return false;
    }
    const uint64_t nBytes = ( ( uint64_t ) ( nOwnerCapacity ) ) * sizeof( sirOwner_t );
    sirOwner_t *pOwners = ( ( sirOwner_t * ) ( pManager->allocateMetadata( nBytes, alignof( sirOwner_t ) ) ) );
    if ( pOwners == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < nOwnerCapacity; ++i )
    {
        new ( pOwners + i ) sirOwner_t();
    }
    m_pOwners = pOwners;
    m_nOwnerCapacity = nOwnerCapacity;
    m_pManager = pManager;
    return true;
}

bool SIRMemoryPressureCoordinator::registerOwner( const sirReclaimOwnerConfig_t& config,
                                                   uint32_t *pIndex )
{
    // Registration must complete before pressure requests or owner services begin.
    const uint32_t nIndex = m_nOwnerCount.load( SIR_MEMORY_ORDER_RELAXED );
    if ( m_pManager == nullptr || pIndex == nullptr || config.pAllocator == nullptr ||
         config.ppSegments == nullptr || config.nSegmentCount == 0 ||
         config.nOwnerType > SIR_RECLAIM_COMPACT_BUCKET ||
         ( config.nOwnerType == SIR_RECLAIM_CACHE_EVICT &&
           config.nMaximumVictims == 0 ) || nIndex >= m_nOwnerCapacity )
    {
        return false;
    }
    for ( uint32_t i = 0; i < config.nSegmentCount; ++i )
    {
        if ( config.ppSegments[i] == nullptr ||
             config.ppSegments[i]->getManager() != m_pManager )
        {
            return false;
        }
    }
    for ( uint32_t k = 0; k < nIndex; ++k )
    {
        const sirOwner_t& existing = m_pOwners[k];
        for ( uint32_t i = 0; i < config.nSegmentCount; ++i )
        {
            for ( uint32_t j = 0; j < existing.nSegmentCount; ++j )
            {
                if ( config.ppSegments[i] == existing.ppSegments[j] )
                {
                    return false;
                }
            }
        }
    }
    SIRMemorySegment **ppCopy = ( ( SIRMemorySegment ** ) ( m_pManager->allocateMetadata( ( ( uint64_t ) ( config.nSegmentCount ) ) *
                                     sizeof( SIRMemorySegment * ),
                                     alignof( SIRMemorySegment * ) ) ) );
    if ( ppCopy == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < config.nSegmentCount; ++i )
    {
        ppCopy[i] = config.ppSegments[i];
    }
    sirOwner_t& owner = m_pOwners[nIndex];
    owner.pAllocator = config.pAllocator;
    owner.ppSegments = ppCopy;
    owner.nSegmentCount = config.nSegmentCount;
    owner.nType = config.nOwnerType;
    owner.nSeat = config.nSeat;
    owner.nWarmSlabs = config.nWarmSlabs;
    owner.nMaximumVictims = config.nMaximumVictims;
    owner.nPriority = config.nPriority;
    *pIndex = nIndex;
    m_nOwnerCount.store( nIndex + 1, SIR_MEMORY_ORDER_RELEASE );
    return true;
}

bool SIRMemoryPressureCoordinator::requestReclaim( uint64_t nTargetBytes )
{
    if ( m_pOwners == nullptr || nTargetBytes == 0 )
    {
        return false;
    }
    SIRScopedLock<SIRMutex> lock( m_RequestMutex );
    m_nRequestedBytes.fetchAdd( nTargetBytes, SIR_MEMORY_ORDER_RELAXED );
    // Cooperative fan-out. Owner lifetimes are fixed; the cold requesting thread
    // never accesses allocation state or invokes a trim callback.
    const uint32_t nCount = m_nOwnerCount.load( SIR_MEMORY_ORDER_ACQUIRE );
    for ( uint32_t i = 0; i < nCount; ++i )
    {
        SIRAtomic<uint64_t>& pending = m_pOwners[i].nPendingBytes;
        uint64_t nOld = pending.load( SIR_MEMORY_ORDER_RELAXED );
        while ( nOld < nTargetBytes &&
                !pending.compareExchangeWeak( nOld, nTargetBytes,
                    SIR_MEMORY_ORDER_RELEASE, SIR_MEMORY_ORDER_RELAXED ) )
        {
        }
    }
    return true;
}

uint64_t SIRMemoryPressureCoordinator::countOwnerCommitted( const sirOwner_t& owner ) const
{
    uint64_t nBytes = 0;
    for ( uint32_t i = 0; i < owner.nSegmentCount; ++i )
    {
        nBytes += owner.ppSegments[i]->getCommittedBytes();
    }
    return nBytes;
}

bool SIRMemoryPressureCoordinator::trimOwner( sirOwner_t& owner, uint64_t nTargetBytes )
{
    switch ( owner.nType )
    {
        case SIR_RECLAIM_COMPACT_BUCKET:
            if ( ( ( SIRCompactBucketAllocator * ) ( owner.pAllocator ) )->getAttachedSeat() ==
                 UINT32_MAX )
            {
                return false;
            }
            ( ( SIRCompactBucketAllocator * ) ( owner.pAllocator ) )->drainRemote();
            ( ( SIRCompactBucketAllocator * ) ( owner.pAllocator ) )->trimEmptySlabs(
                owner.nWarmSlabs );
            return true;
        case SIR_RECLAIM_SLAB:
            return ( ( SIRSlabAllocator * ) ( owner.pAllocator ) )->trimEmptySlabsTo(
                owner.nWarmSlabs );
        case SIR_RECLAIM_SIZE_CLASSES:
            return ( ( SIRReclaimableSizeClassAllocator * ) ( owner.pAllocator ) )->trimIdle();
        case SIR_RECLAIM_SIZE_CLASSES_PRESSURE:
            return ( ( SIRReclaimableSizeClassAllocator * ) ( owner.pAllocator ) )->trimTo( 0 );
        case SIR_RECLAIM_THREAD_SEAT:
            return ( ( SIRThreadLocalAllocator * ) ( owner.pAllocator ) )->trimSeat(
                owner.nSeat, owner.nWarmSlabs );
        case SIR_RECLAIM_BUDDY:
            return ( ( SIRBuddyAllocator * ) ( owner.pAllocator ) )->trimUnusedPages();
        case SIR_RECLAIM_CACHE:
            return ( ( SIRCacheAllocator * ) ( owner.pAllocator ) )->trimUnusedPages();
        case SIR_RECLAIM_HANDLE:
            return ( ( SIRHandleHeap * ) ( owner.pAllocator ) )->trimUnusedPages();
        case SIR_RECLAIM_FREE_LIST:
            return ( ( SIRFreeListAllocator * ) ( owner.pAllocator ) )->trimUnusedPages();
        case SIR_RECLAIM_TLSF:
            return ( ( SIRTLSFAllocator * ) ( owner.pAllocator ) )->trimUnusedPages();
        case SIR_RECLAIM_POOL:
        {
            SIRPoolAllocator *pPool = ( ( SIRPoolAllocator * ) ( owner.pAllocator ) );
            return pPool->getLiveSlots() == 0 && pPool->reset( true );
        }
        case SIR_RECLAIM_FRAME:
            return ( ( SIRFrameAllocator * ) ( owner.pAllocator ) )->trimIdle();
        case SIR_RECLAIM_CACHE_EVICT:
            ( ( SIRCacheAllocator * ) ( owner.pAllocator ) )->reclaimCold(
                nTargetBytes, owner.nMaximumVictims );
            return true;
        case SIR_RECLAIM_RING:
            return ( ( SIRRingAllocator * ) ( owner.pAllocator ) )->trimIfEmpty();
        default:
            return false;
    }
}

uint64_t SIRMemoryPressureCoordinator::serviceOwner( uint32_t nIndex )
{
    if ( m_pOwners == nullptr || nIndex >= m_nOwnerCount.load( SIR_MEMORY_ORDER_ACQUIRE ) )
    {
        return 0;
    }
    sirOwner_t& owner = m_pOwners[nIndex];
    uint64_t nRequested = 0;
    {
        SIRScopedLock<SIRMutex> lock( m_RequestMutex );
        if ( owner.nServicing.exchange( 1, SIR_MEMORY_ORDER_ACQUIRE ) != 0 )
        {
            return 0;
        }
        nRequested = owner.nPendingBytes.exchange( 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
    }
    uint64_t nReclaimed = 0;
    if ( nRequested != 0 )
    {
        const uint64_t nBefore = countOwnerCommitted( owner );
        if ( trimOwner( owner, nRequested ) )
        {
            const uint64_t nAfter = countOwnerCommitted( owner );
            nReclaimed = nBefore > nAfter ? nBefore - nAfter : 0;
        }
        else
        {
            uint64_t nOld = owner.nPendingBytes.load( SIR_MEMORY_ORDER_RELAXED );
            while ( nOld < nRequested &&
                    !owner.nPendingBytes.compareExchangeWeak( nOld, nRequested,
                        SIR_MEMORY_ORDER_RELEASE, SIR_MEMORY_ORDER_RELAXED ) )
            {
            }
        }
        m_nReclaimedBytes.fetchAdd( nReclaimed, SIR_MEMORY_ORDER_RELAXED );
    }
    owner.nServicing.store( 0, SIR_MEMORY_ORDER_RELEASE );
    return nReclaimed;
}

uint32_t SIRMemoryPressureCoordinator::findNextPendingOwner( uint32_t nMaxPriority ) const
{
    if ( m_pOwners == nullptr )
    {
        return UINT32_MAX;
    }
    const uint32_t nCount = m_nOwnerCount.load( SIR_MEMORY_ORDER_ACQUIRE );
    uint32_t nChosen = UINT32_MAX;
    uint32_t nPriority = UINT32_MAX;
    for ( uint32_t i = 0; i < nCount; ++i )
    {
        const sirOwner_t& owner = m_pOwners[i];
        if ( owner.nPriority <= nMaxPriority && owner.nPriority <= nPriority &&
             owner.nPendingBytes.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 )
        {
            if ( owner.nPriority < nPriority || nChosen == UINT32_MAX )
            {
                nChosen = i;
                nPriority = owner.nPriority;
            }
        }
    }
    return nChosen;
}

sirPressureStats_t SIRMemoryPressureCoordinator::getStats( void ) const
{
    SIRScopedLock<SIRMutex> lock( m_RequestMutex );
    sirPressureStats_t stats = {};
    stats.nRequestedBytes = m_nRequestedBytes.load( SIR_MEMORY_ORDER_RELAXED );
    stats.nReclaimedBytes = m_nReclaimedBytes.load( SIR_MEMORY_ORDER_RELAXED );
    stats.nCanceledBytes = m_nCanceledBytes.load( SIR_MEMORY_ORDER_RELAXED );
    stats.nOwnerCount = m_nOwnerCount.load( SIR_MEMORY_ORDER_ACQUIRE );
    for ( uint32_t i = 0; i < stats.nOwnerCount; ++i )
    {
        const uint64_t nPending = m_pOwners[i].nPendingBytes.load( SIR_MEMORY_ORDER_ACQUIRE );
        stats.nPendingOwners += nPending != 0;
        // Owners receive one target each; this is pending owner-work, not
        // historical requested-minus-reclaimed traffic.
        stats.nUnfulfilledBytes += nPending;
    }
    return stats;
}

bool SIRMemoryPressureCoordinator::cancelPending( void )
{
    SIRScopedLock<SIRMutex> lock( m_RequestMutex );
    if ( m_pOwners == nullptr )
    {
        return false;
    }
    const uint32_t nCount = m_nOwnerCount.load( SIR_MEMORY_ORDER_ACQUIRE );
    for ( uint32_t i = 0; i < nCount; ++i )
    {
        if ( m_pOwners[i].nServicing.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 )
        {
            return false;
        }
    }
    uint64_t nCanceled = 0;
    for ( uint32_t i = 0; i < nCount; ++i )
    {
        nCanceled += m_pOwners[i].nPendingBytes.exchange( 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
    }
    m_nCanceledBytes.fetchAdd( nCanceled, SIR_MEMORY_ORDER_RELAXED );
    return true;
}

bool SIRMemoryPressureCoordinator::shutdown( void )
{
    if ( m_pOwners == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < m_nOwnerCount.load( SIR_MEMORY_ORDER_RELAXED ); ++i )
    {
        if ( m_pOwners[i].nServicing.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ||
             m_pOwners[i].nPendingBytes.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 )
        {
            return false;
        }
    }
    // Caller must ensure no outstanding pressure requests and quiescence.
    m_pOwners = nullptr;
    m_pManager = nullptr;
    m_nOwnerCapacity = 0;
    m_nOwnerCount.store( 0, SIR_MEMORY_ORDER_RELAXED );
    return true;
}

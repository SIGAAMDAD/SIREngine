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
#include <Core/Memory/Allocators/SIRThreadLocalAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <new>

typedef struct sirThreadSeatBinding_s {
    SIRThreadLocalAllocator *pAllocator;
    void *pSeat;
    uint64_t nOwner;
    uint32_t nSeat;
    uint32_t nGeneration;
} sirThreadSeatBinding_t;

static SIRAtomic<uint64_t> g_nNextThreadOwner{ 1 };
static thread_local sirThreadSeatBinding_t s_ThreadSeat = { nullptr, nullptr, 0, UINT32_MAX, 0 };
static thread_local uint64_t s_nLocalOwner = 0;

static uint64_t sirGetCurrentThreadOwner( void )
{
    if ( s_nLocalOwner == 0 )
    {
        s_nLocalOwner = g_nNextThreadOwner.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
    }
    return s_nLocalOwner;
}

sirSeat_s::sirSeat_s( void )
    : pSegments{}, pSlotClaims{}, pInbox( nullptr ), nState( 0 ),
      nGeneration( 1 ), nOwnerThread( 0 ), nLiveBytes( 0 ),
      nPendingBytes( 0 ), nAllocations( 0 ), nAllocationFailures( 0 ),
      nInboxCount( 0 ), nInboxInFlight( 0 ), nInitialized( 0 )
{
    InboxLock.clear( SIR_MEMORY_ORDER_RELAXED );
}

SIRThreadLocalAllocator::SIRThreadLocalAllocator( void )
    : m_pManager( nullptr ), m_pSeats( nullptr ), m_nSeatCount( 0 ),
      m_nInboxCapacity( 0 ), m_nAlignment( 16 ), m_nRemoteAccepted( 0 ),
      m_nRemoteRejected( 0 ), m_nRemoteDrained( 0 )
{
}

void SIRThreadLocalAllocator::lockInbox( sirSeat_t& seat )
{
    while ( seat.InboxLock.testAndSet( SIR_MEMORY_ORDER_ACQUIRE ) )
    {
    }
}

void SIRThreadLocalAllocator::unlockInbox( sirSeat_t& seat )
{
    seat.InboxLock.clear( SIR_MEMORY_ORDER_RELEASE );
}

bool SIRThreadLocalAllocator::init( SIRMemoryManager *pManager,
                                     SIRMemorySegment *const *ppSegments,
                                     uint32_t nSeats, uint32_t nInboxCapacity,
                                     uint64_t nSlabBytes, uint32_t nAlignment )
{
    if ( m_pManager != nullptr || pManager == nullptr || ppSegments == nullptr ||
         nSeats == 0 || nInboxCapacity == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) || nAlignment > 32 ||
         nSeats > UINT32_MAX / SIR_CLASSES ||
         ( ( uint64_t ) ( nSeats ) ) * nInboxCapacity >
            UINT64_MAX / sizeof( sirRemoteFree_t ) )
    {
        return false;
    }
    for ( uint32_t i = 0; i < nSeats * SIR_CLASSES; ++i )
    {
        if ( ppSegments[i] == nullptr || ppSegments[i]->getManager() != pManager )
        {
            return false;
        }
        for ( uint32_t j = 0; j < i; ++j )
        {
            if ( ppSegments[i] == ppSegments[j] )
            {
                return false;
            }
        }
        const uint32_t nBytes = SIRSizeClassAllocator::getClassBytes( i % SIR_CLASSES );
        if ( SIRSlabAllocator::getRequiredMetadataBytes(
                ppSegments[i]->getReservedBytes(), nSlabBytes, nBytes, nAlignment ) == 0 )
        {
            return false;
        }
    }
    const uint64_t nSeatsBytes = ( ( uint64_t ) ( nSeats ) ) * sizeof( sirSeat_t );
    const uint64_t nQueueBytes = ( ( uint64_t ) ( nSeats ) ) * nInboxCapacity *
                                 sizeof( sirRemoteFree_t );
    // Permanent manager metadata. Failure of a later allocation/Init is a cold
    // unpublished-layout failure: manager must be discarded, not retried in place.
    void *pSeats = pManager->allocateMetadata( nSeatsBytes, alignof( sirSeat_t ) );
    void *pQueue = pManager->allocateMetadata( nQueueBytes, alignof( sirRemoteFree_t ) );
    if ( pSeats == nullptr || pQueue == nullptr )
    {
        return false;
    }
    sirSeat_t *pArray = ( ( sirSeat_t * ) ( pSeats ) );
    for ( uint32_t i = 0; i < nSeats; ++i )
    {
        new ( pArray + i ) sirSeat_t();
        sirSeat_t& seat = pArray[i];
        seat.pInbox = ( ( sirRemoteFree_t * ) ( pQueue ) ) +
                        ( ( uint64_t ) ( i ) ) * nInboxCapacity;
        for ( uint32_t c = 0; c < SIR_CLASSES; ++c )
        {
            SIRMemorySegment *pSegment = ppSegments[i * SIR_CLASSES + c];
            seat.pSegments[c] = pSegment;
            if ( !seat.Classes[c].init( pSegment,
                   SIRSizeClassAllocator::getClassBytes( c ), nAlignment, nSlabBytes ) )
            {
                // No published seats. Caller discards manager on failed provision.
                return false;
            }
            const uint64_t nCapacity = seat.Classes[c].getSlotIndexCapacity();
            if ( nCapacity == 0 || nCapacity > UINT64_MAX / sizeof( SIRAtomic<uint8_t> ) )
            {
                return false;
            }
            void *pClaims = pManager->allocateMetadata(
                nCapacity * sizeof( SIRAtomic<uint8_t> ), alignof( SIRAtomic<uint8_t> ) );
            if ( pClaims == nullptr )
            {
                return false;
            }
            seat.pSlotClaims[c] = ( ( SIRAtomic<uint8_t> * ) ( pClaims ) );
            for ( uint64_t j = 0; j < nCapacity; ++j )
            {
                new ( seat.pSlotClaims[c] + j ) SIRAtomic<uint8_t>( 0 );
            }
            ++seat.nInitialized;
        }
    }
    m_pManager = pManager;
    m_pSeats = pArray;
    m_nSeatCount = nSeats;
    m_nInboxCapacity = nInboxCapacity;
    m_nAlignment = nAlignment;
    return true;
}

bool SIRThreadLocalAllocator::ownsThread( const sirSeat_t& seat ) const
{
    return seat.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) == 1 &&
           seat.nOwnerThread == sirGetCurrentThreadOwner();
}

bool SIRThreadLocalAllocator::attachThread( uint32_t nSeat )
{
    if ( m_pSeats == nullptr || nSeat >= m_nSeatCount ||
         s_ThreadSeat.pAllocator != nullptr )
    {
        return false;
    }
    sirSeat_t& seat = m_pSeats[nSeat];
    uint32_t nExpected = 0;
    if ( !seat.nState.compareExchangeStrong( nExpected, 1,
             SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) )
    {
        return false;
    }
    seat.nOwnerThread = sirGetCurrentThreadOwner();
    s_ThreadSeat = { this, &seat, seat.nOwnerThread, nSeat,
                     seat.nGeneration.load( SIR_MEMORY_ORDER_RELAXED ) };
    return true;
}

sirThreadSeatToken_t SIRThreadLocalAllocator::getSeatToken( void ) const
{
    if ( s_ThreadSeat.pAllocator != this )
    {
        return { UINT32_MAX, 0 };
    }
    return { s_ThreadSeat.nSeat, s_ThreadSeat.nGeneration };
}

bool SIRThreadLocalAllocator::detachThread( void )
{
    if ( s_ThreadSeat.pAllocator != this )
    {
        return false;
    }
    sirSeat_t& seat = m_pSeats[s_ThreadSeat.nSeat];
    if ( !ownsThread( seat ) )
    {
        return false;
    }
    lockInbox( seat );
    if ( seat.nLiveBytes != 0 || seat.nInboxCount != 0 || seat.nInboxInFlight != 0 )
    {
        unlockInbox( seat );
        return false;
    }
    seat.nOwnerThread = 0;
    seat.nGeneration.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
    s_ThreadSeat = { nullptr, nullptr, 0, UINT32_MAX, 0 };
    seat.nState.store( 0, SIR_MEMORY_ORDER_RELEASE );
    unlockInbox( seat );
    return true;
}

void *SIRThreadLocalAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
    if ( s_ThreadSeat.pAllocator != this || nAlignment > m_nAlignment ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return nullptr;
    }
    sirSeat_t& seat = *( ( sirSeat_t * ) ( s_ThreadSeat.pSeat ) );
    const uint32_t nClass = SIRSizeClassAllocator::getClassIndex( nBytes );
    if ( nClass >= SIR_CLASSES )
    {
        return nullptr;
    }
    void *pMemory = seat.Classes[nClass].alloc();
    if ( pMemory != nullptr )
    {
        uint64_t nSlot = 0;
        if ( !seat.Classes[nClass].getSlotIndex( pMemory, &nSlot ) )
        {
            return nullptr;
        }
        seat.pSlotClaims[nClass][nSlot].store( 1, SIR_MEMORY_ORDER_RELEASE );
        seat.nLiveBytes += nBytes;
        ++seat.nAllocations;
    }
    else
    {
        ++seat.nAllocationFailures;
        drainRemote( m_nInboxCapacity );
        pMemory = seat.Classes[nClass].alloc();
        if ( pMemory != nullptr )
        {
            uint64_t nSlot = 0;
            if ( !seat.Classes[nClass].getSlotIndex( pMemory, &nSlot ) )
            {
                return nullptr;
            }
            seat.pSlotClaims[nClass][nSlot].store( 1, SIR_MEMORY_ORDER_RELEASE );
            seat.nLiveBytes += nBytes;
            ++seat.nAllocations;
        }
    }
    return pMemory;
}

bool SIRThreadLocalAllocator::freeLocal( sirSeat_t& seat, void *pMemory,
                                         uint32_t nBytes, bool bRemote )
{
    const uint32_t nClass = SIRSizeClassAllocator::getClassIndex( nBytes );
    uint64_t nSlot = 0;
    if ( nClass >= SIR_CLASSES || seat.nLiveBytes < nBytes ||
         !seat.Classes[nClass].getSlotIndex( pMemory, &nSlot ) )
    {
        return false;
    }
    SIRAtomic<uint8_t>& claim = seat.pSlotClaims[nClass][nSlot];
    uint8_t nExpected = bRemote ? 2 : 1;
    if ( !claim.compareExchangeStrong( nExpected, 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) )
    {
        return false;
    }
    if ( !seat.Classes[nClass].free( pMemory ) )
    {
        claim.store( bRemote ? 2 : 1, SIR_MEMORY_ORDER_RELEASE );
        return false;
    }
    seat.nLiveBytes -= nBytes;
    --seat.nAllocations;
    return true;
}

bool SIRThreadLocalAllocator::free( void *pMemory, uint32_t nBytes,
                                    uint32_t nAlignment )
{
    if ( s_ThreadSeat.pAllocator != this || nAlignment > m_nAlignment ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return false;
    }
    sirSeat_t& seat = *( ( sirSeat_t * ) ( s_ThreadSeat.pSeat ) );
    return freeLocal( seat, pMemory, nBytes );
}

bool SIRThreadLocalAllocator::queueRemoteFree( sirThreadSeatToken_t token,
                                                void *pMemory, uint32_t nBytes,
                                                uint32_t nAlignment )
{
    if ( m_pSeats == nullptr || token.nSeat >= m_nSeatCount ||
         pMemory == nullptr || nAlignment > m_nAlignment ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) )
    {
        return false;
    }
    const uint32_t nClass = SIRSizeClassAllocator::getClassIndex( nBytes );
    if ( nClass >= SIR_CLASSES )
    {
        return false;
    }
    sirSeat_t& seat = m_pSeats[token.nSeat];
    lockInbox( seat );
    uint64_t nSlot = 0;
    const bool bValid = seat.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) == 1 &&
                        seat.nGeneration.load( SIR_MEMORY_ORDER_RELAXED ) == token.nGeneration &&
                        seat.nInboxCount + seat.nInboxInFlight < m_nInboxCapacity &&
                        seat.Classes[nClass].getSlotIndex( pMemory, &nSlot );
    if ( !bValid )
    {
        unlockInbox( seat );
        m_nRemoteRejected.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
        return false;
    }
    uint8_t nExpected = 1;
    if ( !seat.pSlotClaims[nClass][nSlot].compareExchangeStrong(
            nExpected, 2, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) )
    {
        unlockInbox( seat );
        m_nRemoteRejected.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
        return false;
    }
    seat.pInbox[seat.nInboxCount++] = { pMemory, nBytes, nAlignment };
    seat.nPendingBytes += nBytes;
    unlockInbox( seat );
    m_nRemoteAccepted.fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
    return true;
}

uint32_t SIRThreadLocalAllocator::drainRemote( uint32_t nMaximum )
{
    if ( s_ThreadSeat.pAllocator != this || nMaximum == 0 )
    {
        return 0;
    }
    sirSeat_t& seat = *( ( sirSeat_t * ) ( s_ThreadSeat.pSeat ) );
    uint32_t nDrained = 0;
    while ( nDrained < nMaximum )
    {
        lockInbox( seat );
        if ( seat.nInboxCount == 0 )
        {
            unlockInbox( seat );
            break;
        }
        const sirRemoteFree_t entry = seat.pInbox[--seat.nInboxCount];
        ++seat.nInboxInFlight;
        unlockInbox( seat );
        // Work belongs to this owner. No producer accesses slab freelists;
        // the queue slot remains reserved until success or restoration.
        const bool bFreed = freeLocal( seat, entry.pMemory, entry.nBytes, true );
        lockInbox( seat );
        --seat.nInboxInFlight;
        if ( bFreed )
        {
            seat.nPendingBytes -= entry.nBytes;
        }
        else
        {
            seat.pInbox[seat.nInboxCount++] = entry;
        }
        unlockInbox( seat );
        if ( !bFreed )
        {
            break;
        }
        ++nDrained;
    }
    m_nRemoteDrained.fetchAdd( nDrained, SIR_MEMORY_ORDER_RELAXED );
    return nDrained;
}

bool SIRThreadLocalAllocator::trimSeat( uint32_t nSeat, uint32_t nWarmPerClass )
{
    if ( m_pSeats == nullptr || nSeat >= m_nSeatCount ||
         !ownsThread( m_pSeats[nSeat] ) ||
         s_ThreadSeat.nSeat != nSeat )
    {
        return false;
    }
    if ( drainRemote() == 0 )
    {
        // No queued work or a failed free; proceed only if queue empty.
    }
    sirSeat_t& seat = m_pSeats[nSeat];
    lockInbox( seat );
    const bool bEmpty = seat.nInboxCount == 0;
    unlockInbox( seat );
    if ( !bEmpty )
    {
        return false;
    }
    for ( uint32_t c = 0; c < SIR_CLASSES; ++c )
    {
        if ( !seat.Classes[c].trimEmptySlabsTo( nWarmPerClass ) )
        {
            return false;
        }
    }
    return true;
}

bool SIRThreadLocalAllocator::shutdown( void )
{
    if ( m_pSeats == nullptr )
    {
        return false;
    }
    for ( uint32_t i = 0; i < m_nSeatCount; ++i )
    {
        sirSeat_t& seat = m_pSeats[i];
        if ( seat.nState.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ||
             seat.nInboxCount != 0 || seat.nLiveBytes != 0 )
        {
            return false;
        }
    }
    for ( uint32_t i = 0; i < m_nSeatCount; ++i )
    {
        sirSeat_t& seat = m_pSeats[i];
        for ( uint32_t c = 0; c < SIR_CLASSES; ++c )
        {
            if ( !seat.Classes[c].shutdown() )
            {
                return false;
            }
        }
        seat.~sirSeat_t();
    }
    m_pSeats = nullptr;
    m_pManager = nullptr;
    m_nSeatCount = 0;
    return true;
}

sirThreadAllocatorStats_t SIRThreadLocalAllocator::getStats( void ) const
{
    sirThreadAllocatorStats_t stats = {};
    if ( m_pSeats == nullptr )
    {
        return stats;
    }
    for ( uint32_t i = 0; i < m_nSeatCount; ++i )
    {
        const sirSeat_t& seat = m_pSeats[i];
        // Cold query must occur at quiescent safepoint; live counters are
        // owner-writable without atomics to keep the allocation path local.
        stats.nLivePayloadBytes += seat.nLiveBytes;
        stats.nRemotePendingBytes += seat.nPendingBytes;
        stats.nAllocationFailures += seat.nAllocationFailures;
        for ( uint32_t c = 0; c < SIR_CLASSES; ++c )
        {
            stats.nRefills += seat.Classes[c].getColdActivations();
        }
        stats.nAllocatedCount += seat.nAllocations;
        stats.nActiveSeats += seat.nState.load( SIR_MEMORY_ORDER_RELAXED ) == 1;
    }
    stats.nRemoteAccepted = m_nRemoteAccepted.load( SIR_MEMORY_ORDER_RELAXED );
    stats.nRemoteRejected = m_nRemoteRejected.load( SIR_MEMORY_ORDER_RELAXED );
    stats.nRemoteDrained = m_nRemoteDrained.load( SIR_MEMORY_ORDER_RELAXED );
    return stats;
}

uint32_t SIRThreadLocalAllocator::getSeatCount( void ) const
{
    return m_nSeatCount;
}

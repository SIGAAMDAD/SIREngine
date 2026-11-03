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

#include <Core/Memory/SIRAllocationRouter.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRTLSFAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/Allocators/SIRFrameAllocator.h>

// Every routed backend is owner-thread only. The routing counters are
// snapshots of this owner thread; cross-thread use is rejected.
static thread_local uint8_t s_nRouterThreadMarker = 0;
static uintptr_t routerThreadToken( void )
{
    return ( ( uintptr_t ) ( &s_nRouterThreadMarker ) );
}


SIRAllocationRouter::SIRAllocationRouter( void )
    : m_Config{}, m_nCounts{}, m_nOwnerToken( 0 ), m_bInitialized( false )
{
}

bool SIRAllocationRouter::init( const sirAllocationRouterConfig_t& config )
{
    if ( m_bInitialized || config.nSmallMaximumBytes == 0 ||
         config.nSmallMaximumBytes > 2048 ||
         config.nLargeMinimumBytes <= config.nSmallMaximumBytes ||
         config.pBuddy == nullptr ||
         ( config.bUseReclaimableSmall ? config.pReclaimableClasses == nullptr :
           config.pPoolClasses == nullptr ) ||
         ( config.bUseTLSFMedium ? config.pTLSF == nullptr :
           config.pFreeList == nullptr ) )
    {
        return false;
    }
    m_Config = config;
    m_nOwnerToken = routerThreadToken();
    m_bInitialized = true;
    return true;
}

bool SIRAllocationRouter::alloc( uint32_t nBytes, uint32_t nAlignment,
                                 sirRoutedAllocation_t *pResult,
                                 sirAllocationLifetime_e nLifetime,
                                 sirJobScratchToken_t job )
{
    if ( !m_bInitialized || m_nOwnerToken != routerThreadToken() || pResult == nullptr || nBytes == 0 ||
         !SIR_IS_POWER_OF_TWO( nAlignment ) ||
         nLifetime > SIR_LIFETIME_FRAME )
    {
        return false;
    }
    void *pMemory = nullptr;
    sirRoutingBacking_e backing = SIR_ROUTE_NONE;
    if ( nLifetime == SIR_LIFETIME_JOB && m_Config.pJobs != nullptr )
    {
        backing = SIR_ROUTE_JOB;
        pMemory = m_Config.pJobs->alloc( job, nBytes, nAlignment );
    }
    else if ( nLifetime == SIR_LIFETIME_FRAME && m_Config.pFrames != nullptr )
    {
        backing = SIR_ROUTE_FRAME;
        pMemory = m_Config.pFrames->alloc( nBytes, nAlignment );
    }
    else if ( nLifetime != SIR_LIFETIME_ARBITRARY )
    {
        return false;
    }
    else if ( nBytes <= m_Config.nSmallMaximumBytes && nAlignment <= 16 )
    {
        if ( m_Config.bUseReclaimableSmall )
        {
            backing = SIR_ROUTE_RECLAIMABLE;
            pMemory = m_Config.pReclaimableClasses->alloc( nBytes, nAlignment );
        }
        else
        {
            backing = SIR_ROUTE_POOL;
            pMemory = m_Config.pPoolClasses->alloc( nBytes );
        }
    }
    else if ( nBytes < m_Config.nLargeMinimumBytes )
    {
        if ( m_Config.bUseTLSFMedium )
        {
            backing = SIR_ROUTE_TLSF;
            pMemory = m_Config.pTLSF->alloc( nBytes, nAlignment );
        }
        else
        {
            backing = SIR_ROUTE_FREE_LIST;
            pMemory = m_Config.pFreeList->alloc( nBytes, nAlignment );
        }
    }
    else
    {
        backing = SIR_ROUTE_BUDDY;
        pMemory = m_Config.pBuddy->alloc( nBytes, nAlignment );
    }
    if ( pMemory == nullptr )
    {
        return false;
    }
    *pResult = { pMemory, nBytes, backing };
    m_nCounts[backing].fetchAdd( 1, SIR_MEMORY_ORDER_RELAXED );
    return true;
}

bool SIRAllocationRouter::free( sirRoutedAllocation_t *pAllocation )
{
    if ( !m_bInitialized || m_nOwnerToken != routerThreadToken() || pAllocation == nullptr || pAllocation->pMemory == nullptr )
    {
        return false;
    }
    bool bFreed = false;
    switch ( pAllocation->nBacking )
    {
        case SIR_ROUTE_POOL:
            bFreed = !m_Config.bUseReclaimableSmall &&
                m_Config.pPoolClasses->free( pAllocation->pMemory, pAllocation->nBytes );
            break;
        case SIR_ROUTE_RECLAIMABLE:
            bFreed = m_Config.bUseReclaimableSmall &&
                m_Config.pReclaimableClasses->free( pAllocation->pMemory, pAllocation->nBytes );
            break;
        case SIR_ROUTE_FREE_LIST:
            bFreed = !m_Config.bUseTLSFMedium &&
                m_Config.pFreeList->free( pAllocation->pMemory );
            break;
        case SIR_ROUTE_TLSF:
            bFreed = m_Config.bUseTLSFMedium &&
                m_Config.pTLSF->free( pAllocation->pMemory );
            break;
        case SIR_ROUTE_BUDDY:
            bFreed = m_Config.pBuddy->free( pAllocation->pMemory );
            break;
        default:
            // Frame and job objects do not have individual free.
            return false;
    }
    if ( bFreed )
    {
        *pAllocation = { nullptr, 0, SIR_ROUTE_NONE };
    }
    return bFreed;
}

uint64_t SIRAllocationRouter::getRoutedCount( sirRoutingBacking_e backing ) const
{
    return m_bInitialized && m_nOwnerToken == routerThreadToken() &&
           backing > SIR_ROUTE_NONE && backing <= SIR_ROUTE_FRAME ?
        m_nCounts[backing].load( SIR_MEMORY_ORDER_RELAXED ) : 0;
}

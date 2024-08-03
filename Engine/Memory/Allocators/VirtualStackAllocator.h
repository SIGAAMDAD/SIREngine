#ifndef __VIRTUAL_STACK_ALLOCATOR_H__
#define __VIRTUAL_STACK_ALLOCATOR_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "AllocatorBase.h"

enum class EVirtualStackAllocatorDecommitMode : uint8_t {
	// Default mode, does not decommit pages until the allocator is destroyed
	AllOnDestruction = 0,
	// Decommits all pages once none are in use
	AllOnStackEmpty = 1,
	// Tracks the high water mark and uses it to free excess memory that is not expected to be used again soon
	// This enables us to quickly release memory consumed by a one-off spikey usage pattern while avoiding frequent page management in the steady state
	// See DecommitUnusedPages() for details of the heuristic used
	ExcessOnStackEmpty = 2,
	NumModes
};

class CVirtualStackAllocator : public IAllocatorBase
{
public:
    CVirtualStackAllocator( const char *pAllocatorName, size_t nRequestedSize, EVirtualStackAllocatorDecommitMode decommitMode );
    virtual ~CVirtualStackAllocator();

    virtual void Init( void ) override
    { }
    virtual void Shutdown( void ) override
    { }

    virtual uint64_t BytesUsed( void ) const override
    { return (byte *)m_pNextAllocationStart - (byte *)m_pMemory; }
    uint64_t CommittedBytes( void )
    { return (byte *)m_pNextUncommittedPage - (byte *)m_pMemory; }

    virtual void *Allocate( uint64_t nBytes, uint64_t nAlignment = 16 ) override;
    virtual void Deallocate( void *pMemory ) override
    { }

    void DecommitUnusedPages( void );
private:
    CVirtualStackAllocator( const CVirtualStackAllocator& ) = delete;
    CVirtualStackAllocator( CVirtualStackAllocator&& ) = delete;
    CVirtualStackAllocator& operator=( const CVirtualStackAllocator& ) = delete;
    CVirtualStackAllocator& operator=( CVirtualStackAllocator&& ) = delete;

    CVirtualStackAllocator( void );

    void *m_pNextUncommittedPage;
    void *m_pNextAllocationStart;

    size_t m_nTotalReservationSize;

    void *m_pMemory;
    size_t m_nVirtualMemorySize;

    void *m_pRecentHighWaterMark;

    EVirtualStackAllocatorDecommitMode m_DecommitMode = EVirtualStackAllocatorDecommitMode::AllOnDestruction;
};

#endif
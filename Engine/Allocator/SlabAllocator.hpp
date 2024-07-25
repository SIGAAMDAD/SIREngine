#ifndef __SIRENGINE_SLAB_ALLOCATOR_HPP__
#define __SIRENGINE_SLAB_ALLOCATOR_HPP__

#pragma once

#include <Engine/Allocator/AllocatorBase.hpp>

class CSlabAllocator : public IAllocatorBase
{
public:
    CSlabAllocator( void );
    CSlabAllocator( uint64_t nBytes );
    virtual ~CSlabAllocator() override;

    virtual void Init( uint64_t nBytes );
    virtual void Shutdown( void ) override;

    virtual void *Allocate( uint64_t nBytes );

    void Reset( void );

    // NOTE: calling this will reallocate the entire memory block, use sparingly
    void Resize( uint64_t nBytes );
private:
    uint64_t m_nSize;
    uint64_t m_nUsed;
    uint8_t *m_pBase;
#if defined(SIRENGINE_MEMORY_DEBUG)
    uint64_t m_nAllocs;
    uint64_t m_nFrees;
#endif
};

#endif
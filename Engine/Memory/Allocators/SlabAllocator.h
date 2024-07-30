#ifndef __SIRENGINE_SLAB_ALLOCATOR_HPP__
#define __SIRENGINE_SLAB_ALLOCATOR_HPP__

#pragma once

#include <Engine/Memory/Allocators/AllocatorBase.h>

template<typename AllocatorType = UtlMallocAllocator>
class CSlabAllocator : public IAllocatorBase
{
public:
    CSlabAllocator( void )
        : IAllocatorBase( "SlabAllocator" ), m_nSize( 0 ), m_nUsed( 0 ), m_pBase( NULL )
    { }
    CSlabAllocator( uint64_t nBytes )
        : IAllocatorBase( "SlabAllocator" ), m_nSize( 0 ), m_nUsed( 0 ), m_pBase( NULL )
    { Init( nBytes ); }
    virtual ~CSlabAllocator() override
    { Shutdown(); }

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
    AllocatorType m_Allocator;
};

#endif
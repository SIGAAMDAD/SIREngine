#ifndef __SIRENGINE_ALLOCATOR_BASE_HPP__
#define __SIRENGINE_ALLOCATOR_BASE_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Memory/Memory.h>

class IAllocatorBase
{
public:
    IAllocatorBase( const char *pAllocatorName )
        : m_pAllocatorName( pAllocatorName )
    { }
    virtual ~IAllocatorBase()
    { }

    virtual void Init( void ) = 0;
    virtual void Shutdown( void ) = 0;

    virtual uint64_t BytesUsed( void ) const = 0;
    virtual const char *GetName( void ) const;

    virtual void *Allocate( uint64_t nBytes, uint64_t nAlignment = 16 ) = 0;
    virtual void Deallocate( void *pMemory ) = 0;
protected:
    const char *m_pAllocatorName;
};

SIRENGINE_FORCEINLINE const char *IAllocatorBase::GetName( void ) const
{
    return m_pAllocatorName;
}

#endif
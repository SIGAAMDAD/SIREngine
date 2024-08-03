#ifndef __MEMALLOC_H__
#define __MEMALLOC_H__

#pragma once

#include "Memory.h"

class IMemAlloc
{
public:
    IMemAlloc( void )
    { }
    virtual ~IMemAlloc()
    { }

    virtual void Shutdown( void ) = 0;

    // Release versions
	virtual void *Alloc( size_t nSize ) = 0;
	virtual void *Realloc( void *pMemory, size_t nSize ) = 0;
	virtual void Free( void *pMemory ) = 0;

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, uint64_t nLineNumber ) = 0;
    virtual void *Realloc( void *pMemory, size_t nSize, const char *pFileName, uint64_t nLineNumber ) = 0;
    virtual void  Free( void *pMemory, const char *pFileName, uint64_t nLineNumber ) = 0;

    virtual size_t GetAllocSize( void *pMemory ) = 0;

    virtual void DumpStats( void ) = 0;
	virtual void DumpStatsFileBase( char const *pchFileBase ) = 0;

    virtual bool IsDebugHeap( void ) const = 0;

    virtual void GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) = 0;
};

extern IMemAlloc *g_pMemAlloc;

#if defined(SIRENGINE_REPLACE_NEW_AND_DELETE)
#include <new>
#undef new
#undef delete

SIRENGINE_FORCEINLINE void *operator new( size_t nSize )
{ return g_pMemAlloc->Alloc( nSize ); }
SIRENGINE_FORCEINLINE void *operator new( size_t nSize, size_t alignment )
{ return g_pMemAlloc->Alloc( SIRENGINE_PAD( nSize, alignment ) ); }

SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize )
{ return ::operator new( nSize ); }
SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, size_t alignment )
{ return ::operator new( nSize, alignment ); }

SIRENGINE_FORCEINLINE void *operator new( size_t nSize, const std::nothrow_t& )
{ return ::operator new( nSize ); }
SIRENGINE_FORCEINLINE void *operator new( size_t nSize, size_t alignment, const std::nothrow_t& )
{ return ::operator new( nSize, alignment ); }

SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, const std::nothrow_t& )
{ return ::operator new[]( nSize ); }
SIRENGINE_FORCEINLINE void *operator new[]( size_t nSize, size_t alignment, const std::nothrow_t& )
{ return ::operator new[]( nSize, alignment ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory ) noexcept
{ g_pMemAlloc->Free( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory ) noexcept
{ ::operator delete( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory, size_t ) noexcept
{ ::operator delete( pMemory ); }
SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory, size_t ) noexcept
{ ::operator delete[]( pMemory ); }

SIRENGINE_FORCEINLINE void operator delete( void *pMemory, size_t, size_t ) noexcept
{ ::operator delete( pMemory ); }
SIRENGINE_FORCEINLINE void operator delete[]( void *pMemory, size_t, size_t ) noexcept
{ ::operator delete[]( pMemory ); }

#endif

#endif
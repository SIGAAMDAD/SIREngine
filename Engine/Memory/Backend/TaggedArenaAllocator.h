#ifndef __TAGGED_ARENA_ALLOCATOR_H__
#define __TAGGED_ARENA_ALLOCATOR_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "../MemAlloc.h"

class CTaggedArenaAllocator : public IMemAlloc
{
public:
    CTaggedArenaAllocator( void );
    virtual ~CTaggedArenaAllocator() override;

    // Release versions
	virtual void *Alloc( size_t nSize ) override;
	virtual void *Realloc( void *pMemory, size_t nSize ) override;
	virtual void Free( void *pMemory ) override;

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, uint64_t nLineNumber ) override;
    virtual void *Realloc( void *pMemory, size_t nSize, const char *pFileName, uint64_t nLineNumber ) override;
    virtual void  Free( void *pMemory, const char *pFileName, uint64_t nLineNumber ) override;

    virtual size_t GetAllocSize( void *pMemory ) override;

    virtual void DumpStats( void ) override;
	virtual void DumpStatsFileBase( char const *pchFileBase ) override;

    virtual bool IsDebugHeap( void ) const override;

    virtual void GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) override;
};

#endif
#ifndef __MEMALLOC_H__
#define __MEMALLOC_H__

#pragma once

#include "Memory.h"

class IMemAlloc
{
public:
    IMemAlloc( void );
    virtual ~IMemAlloc();

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

    virtual void OutOfMemory( size_t nBytesAttempted = 0 ) = 0;

    virtual void GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) = 0;
};

extern IMemAlloc *g_pMemAlloc;

#endif
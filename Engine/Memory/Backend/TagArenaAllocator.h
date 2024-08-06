#ifndef __SIRENGINE_TAG_ARENA_ALLOCATOR_H__
#define __SIRENGINE_TAG_ARENA_ALLOCATOR_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "../Memory.h"
#include "../MemAlloc.h"
#include <Engine/Util/CString.h>
#include <EASTL/unordered_map.h>

#define USE_MEMSTATIC 1
#define USE_MULTI_SEGMENT 1
#define USE_TRASH_TEST 1

#define RETRYAMOUNT (256*1024)

#define MEM_ALIGN		64
#define MIN_FRAGMENT	64

#if 1 // forward lookup, faster allocation
#define DIRECTION next
// we may have up to 4 lists to group free blocks by size
//#define TINY_SIZE	32
#define SMALL_SIZE	64
#define MEDIUM_SIZE	128
#else // backward lookup, better free space consolidation
#define DIRECTION prev
#define TINY_SIZE	64
#define SMALL_SIZE	128
#define MEDIUM_SIZE	256
#endif

#define TAG_FREE 0
#define TAG_STATIC 1

typedef enum {
    // sets a deallocated block's memory to scramble to weed out bugs
    ARENA_SCRAMBLE_ON_DEALLOC   = 0x10,
    
    // initializes all allocations to 0
    ARENA_INIT_ON_ALLOC         = 0x20,

} ArenaAllocOptions_t;

typedef struct memzone_s memzone_t;

class CTagArenaAllocator : public IMemAlloc
{
public:
    CTagArenaAllocator( const char *pName, uint64_t nSize, uint64_t nFlags = ARENA_INIT_ON_ALLOC | ARENA_SCRAMBLE_ON_DEALLOC );
    virtual ~CTagArenaAllocator() override;

	virtual void Shutdown( void ) override;

    // Release versions
	virtual void *Alloc( size_t nSize ) override;
	virtual void *Realloc( void *pMemory, size_t nSize ) override;
	virtual void Free( void *pMemory ) override;

    virtual void *Alloc( size_t nSize, uint64_t nTag );
	virtual void *Realloc( void *pMemory, size_t nSize, uint64_t nTag );

	// Debug versions
    virtual void *Alloc( size_t nSize, const char *pFileName, uint64_t nLineNumber ) override;
    virtual void *Realloc( void *pMemory, size_t nSize, const char *pFileName, uint64_t nLineNumber ) override;
    virtual void  Free( void *pMemory, const char *pFileName, uint64_t nLineNumber ) override;

    virtual size_t GetAllocSize( void *pMemory ) override;

    virtual void DumpStats( void ) override;
	virtual void DumpStatsFileBase( char const *pchFileBase ) override;

    virtual bool IsDebugHeap( void ) const override;

    virtual void GetMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) override;

    uint64_t AllocateTagGroup( const char *pName );
    void ClearTagGroup( uint64_t nTag );

    static int nMinFragment;
    static uint64_t nPageSize;
private:
    const char *m_pName;
    memzone_t *m_pZone;
    eastl::unordered_map<uint64_t, CString> m_TagList;
};

#endif
#ifndef __SIRENGINE_TAG_ARENA_ALLOCATOR_H__
#define __SIRENGINE_TAG_ARENA_ALLOCATOR_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "../MemAlloc.h"


#define USE_MEMSTATIC 1
#define USE_MULTI_SEGMENT 1
#define USE_TRASH_TEST 1

#define RETRYAMOUNT (256*1024)

#define MEM_ALIGN		64
#define MIN_FRAGMENT	64

#if defined(USE_MULTI_SEGMENT)
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
#endif

#define TAG_FREE 0
#define TAG_STATIC 1

typedef struct memblock_s {
	struct memblock_s	*next, *prev;
	uint64_t	size;	// including the header and possibly tiny fragments
	uint32_t	tag;	// a tag of 0 is a free block
	uint32_t	id;		// should be ZONEID
#if defined(SIRENGINE_MEMORY_DEBUG)
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	uint64_t	size;			// total bytes malloced, including header
	uint64_t	used;			// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
#if defined(USE_MULTI_SEGMENT)
	memblock_t	dummy0;		// just to allocate some space before freelist
	freeblock_t	freelist_tiny;
	memblock_t	dummy1;
	freeblock_t	freelist_small;
	memblock_t	dummy2;
	freeblock_t	freelist_medium;
	memblock_t	dummy3;
	freeblock_t	freelist;
#else
	memblock_t	*rover;
#endif
} memzone_t;

class CTagArenaAllocator : public IMemAlloc
{
public:
    CTagArenaAllocator( const char *pName, uint64_t nSize );
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

    void AllocateTagGroup( const char *pName, uint64_t nTag );
    void ClearTagGroup( uint64_t nTag );

    static int nMinFragment;
    static uint64_t nPageSize;
private:
    const char *m_pName;
    memzone_t *m_pZone;

    eastl::unordered_map<uint64_t, CString> m_TagList;
};

#endif
#ifndef __CACHED_OS_PAGE_ALLOCATOR_H__
#define __CACHED_OS_PAGE_ALLOCATOR_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "../MemAlloc.h"

class ICachedOSPageAllocator
{
protected:
    typedef struct FreePageBlock {
        void *pBuffer;
        size_t nByteSize;

        FreePageBlock()
        {
            pBuffer = NULL;
            nByteSize = 0;
        }
    } FreePageBlock_t;

    void *AllocateImpl( size_t nSize, size_t nCachedByteLimit, FreePageBlock_t *pFirst,
        FreePageBlock_t *pLast, uint32_t& nFreedPageBlocksNum, size_t& nCachedTotal, void *pMutex = NULL );
    void FreeImpl( void *pBuffer, size_t nSize, uint32_t nCacheBlocks, uint32_t nCachedByteLimit,
        FreePageBlock_t *pFirst, uint32_t& nFreedPageBlocksNum, size_t& nCachedTotal, void *pMutex = NULL,
        bool ThreadIsTimeCritical = false );
	void FreeAllImpl( FreePageBlock_t *pFirst, uint32_t& nFreedPageBlocksNum, size_t& nCachedTotal, void *pMutex = NULL );

    static bool IsOSAllocation( size_t nSize, uint32_t nCachedByteLimit )
    {
        return ( nSize > nCachedByteLimit / 4 );
    }
};

template<uint32_t NumCacheBlocks, uint32_t CachedBytesLimit>
class CCachedOSPageAllocator : public ICachedOSPageAllocator
{
public:
    CCachedOSPageAllocator( void )
        : m_nCachedTotal( 0 ), m_nFreedPageBlocksNum( 0 )
    { }
    
    SIRENGINE_FORCEINLINE void *Alloc( uint64_t nSize, uint32_t nAllocationHint = 0, void *pMutex = NULL )
    {
        return AllocateImpl( nSize, CachedBytesLimit, m_szFreedPageBlocks, m_szFreedPageBlocks + m_nFreedPageBlocksNum,
            m_nFreedPageBlocksNum, m_nCachedTotal, pMutex );
    }

    void Free( void *pBuffer, size_t nSize, void *pMutex = NULL, bool bThreadIsTimeCritical = false )
    {
        FreeImpl( pBuffer, nSize, bThreadIsTimeCritical ? NumCacheBlocks * 2 : NumCacheBlocks, CachedBytesLimit,
            m_szFreedPageBlocks, m_nFreedPageBlocksNum, m_nCachedTotal, pMutex, bThreadIsTimeCritical );
    }
    
    void FreeAll( void *pMutex = NULL )
    {
        FreeAllImpl( m_szFreedPageBlocks, m_nFreedPageBlocksNum, m_nCachedTotal, pMutex );
    }

    SIRENGINE_FORCEINLINE uint64_t GetCachedFreeTotal( void )
	{
		return CachedTotal;
	}    
private:
    FreePageBlock_t m_szFreedPageBlocks[ NumCacheBlocks * 2 ];
    size_t m_nCachedTotal;
    uint32_t m_nFreedPageBlocksNum;
};

#endif
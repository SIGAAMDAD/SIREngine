#include "CachedOSPageAllocator.h"

void *ICachedOSPageAllocator::AllocateImpl( size_t nSize, size_t nCachedByteLimit, FreePageBlock_t *pFirst,
        FreePageBlock_t *pLast, uint32_t& nFreedPageBlocksNum, size_t& nCachedTotal, void *pMutex = NULL )
{
    if ( !IsOSAllocation( nSize, nCachedByteLimit ) ) {
        if ( pFirst != pLast ) {
            FreePageBlock_t *pFound = NULL;

            for ( FreePageBlock_t *pBlock = pFirst; pBlock != pLast; ++pBlock ) {
                // look for exact matches first, these are aligned to the page size, so it should be quite common to hit these on small pages sizes
				if ( pBlock->nByteSize == nSize ) {
					pFound = pBlock;
					break;
				}
            }

            if ( pFound ) {
                void *pResult = pFound->pBuffer;
                nCachedTotal -= pFound->nByteSize;
                if ( pFound + 1 != pLast ) {
                    memmove( pFound, pFound + 1, sizeof( FreePageBlock_t ) * ( ( pLast - pFound ) - 1 ) );
                }
                --nFreedPageBlocksNum;
                return pResult;
            }
        {
            void *pResult = NULL;
            {

            }
        }
        }
    }
}
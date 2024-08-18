#ifndef __SIRENGINE_FILECACHE_H__
#define __SIRENGINE_FILECACHE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/Types.h>
#include "FilePath.h"
#include <EASTL/unordered_map.h>

namespace SIREngine::FileSystem {
    typedef struct FileCacheEntry {
        void *pMemory;
        size_t nSize;
        void *hFile;
    } FileCacheEntry_t;

    typedef struct {
        uint64_t nTotalCachedBytes;
        uint64_t nTotalBytesInUse;
        uint64_t nTotalHandles;
    } FileCacheStats_t;

    class CFileCache
    {
    public:
        CFileCache( const CFilePath& directory );
        ~CFileCache();

        inline FileCacheEntry_t *GetFile( const CFilePath& filePath )
        {
            auto it = m_CacheList.find( filePath );
            if ( it != m_CacheList.cend() ) {
                return &it->second;
            }
            return NULL;
        }

        void LoadCachedFile( const CFilePath& filePath );
        void ReleaseCachedFile( const CFilePath& filePath );
    private:
        void AllocateCache( const CFilePath& directory );
        void MapFile( const CFilePath& filePath, FileCacheEntry_t *pCacheEntry );

        eastl::unordered_map<CFilePath, FileCacheEntry_t> m_CacheList;
    };
};

#endif
#include "FileSystem.h"
#include "FileCache.h"
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>

using namespace FileSystem;

CFileCache::CFileCache( const FileSystem::CFilePath& directory )
{
    AllocateCache( directory );
}

CFileCache::~CFileCache()
{
    for ( auto& it : m_CacheList ) {
        if ( it.second.hFile == (void *)SIRENGINE_INVALID_HANDLE ) {
            continue;
        }
        g_pApplication->UnmapFile( it.second.pMemory, it.second.nSize );
        g_pApplication->FileClose( (void *)it.second.hFile );
    }
}

void CFileCache::AllocateCache( const FileSystem::CFilePath& directory )
{
    size_t i, nCachedBytes;
    FileCacheEntry_t *pCacheEntry;
    CVector<CFilePath> fileList = eastl::move( g_pApplication->ListFiles( directory, false ) );

    SIRENGINE_LOG( "Allocating file cache in directory \"%s\"", directory.c_str() );

    m_CacheList.reserve( fileList.size() );
    nCachedBytes = 0;

    for ( i = 0; i < fileList.size(); i++ ) {
        pCacheEntry = &m_CacheList.try_emplace( fileList[i] ).first->second;
        MapFile( fileList[i], pCacheEntry );
        nCachedBytes += pCacheEntry->nSize;
    }

    SIRENGINE_LOG( "Cached %lu bytes total", nCachedBytes );
}

void CFileCache::MapFile( const FileSystem::CFilePath& filePath, FileCacheEntry_t *pCacheEntry )
{
    pCacheEntry->hFile = g_pApplication->FileOpen( filePath, FileMode_ReadOnly );

    SIRENGINE_LOG( "Caching file \"%s\"...", filePath.c_str() );

    if ( (PlatformTypes::file_t)(uintptr_t)pCacheEntry->hFile == SIRENGINE_INVALID_HANDLE ) {
        SIRENGINE_WARNING( "Error opening file" );
        memset( pCacheEntry, 0, sizeof( *pCacheEntry ) );
        return;
    }

    pCacheEntry->pMemory = g_pApplication->MapFile( pCacheEntry->hFile, &pCacheEntry->nSize );
    if ( !pCacheEntry->pMemory ) {
        SIRENGINE_WARNING( "Error mapping file" );
        g_pApplication->FileClose( pCacheEntry->hFile );
        return;
    }
}
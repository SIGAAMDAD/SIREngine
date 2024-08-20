#include "FileSystem.h"
#include "FileCache.h"
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>

#if defined(SIRENGINE_NO_FILE_MAPPING)
	#pragma message("SIRENGINE_NO_FILE_MAPPING defined, this will use a lot more memory")
#endif

using namespace SIREngine;
using namespace SIREngine::Application;

namespace SIREngine::FileSystem {

SIRENGINE_DEFINE_LOG_CATEGORY( FileCache, ELogLevel::Info );

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
	#if defined(SIRENGINE_NO_FILE_MAPPING)
		delete[] (char *)it.second.pMemory;
	#else
		Application::Get()->UnmapFile( it.second.pMemory, it.second.nSize );
		Application::Get()->FileClose( it.second.hFile );
	#endif
	}
}

FileCacheEntry_t *CFileCache::GetFile( const CFilePath& filePath )
{
	CThreadAutoLock<CThreadMutex> _( m_hLock );
	auto it = m_CacheList.find( filePath );
	if ( it != m_CacheList.cend() ) {
		if ( it->second.hFile == (void *)SIRENGINE_INVALID_HANDLE ) {
			LoadCachedFile( filePath );
		}
		return &it->second;
	}
	return NULL;
}

void CFileCache::LoadCachedFile( const CFilePath& filePath )
{
	FileCacheEntry_t *pCacheEntry;

	CThreadAutoLock<CThreadMutex> _( m_hLock );
	auto it = m_CacheList.find( filePath );
    if ( it == m_CacheList.cend() ) {
		pCacheEntry = &m_CacheList.try_emplace( filePath ).first->second;
    } else {
		pCacheEntry = &it->second;
	}

	if ( pCacheEntry->hFile != (void *)SIRENGINE_INVALID_HANDLE || pCacheEntry->pMemory != NULL ) {
		SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Warning, "CFileCache::LoadCachedFile: resource \"%s\" already loaded (remapping)", filePath.c_str() );
		ReleaseCachedFile( filePath );
	}
	MapFile( filePath, pCacheEntry );
}

void CFileCache::ReleaseCachedFile( const CFilePath& filePath )
{
	CThreadAutoLock<CThreadMutex> _( m_hLock );
    auto it = m_CacheList.find( filePath );
    if ( it == m_CacheList.cend() ) {
		SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Warning, "CFileCache::ReleaseCachedFile: invalid path \"%s\", no such file!", filePath.c_str() );
		return;
    }
	if ( it->second.hFile == (void *)SIRENGINE_INVALID_HANDLE ) {
		SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Warning, "CFileCache::ReleaseCachedFile: cached file \"%s\" already released.", filePath.c_str() );
		return;
	}

	if ( !it->second.nSize ) {
		// empty file
		it->second.hFile = (void *)SIRENGINE_INVALID_HANDLE;
		return;
	}

	SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Verbose, "Unmapping cached file \"%s\"...", filePath.c_str() );
#if defined(SIRENGINE_NO_FILE_MAPPING)
	delete[] (char *)it.second.pMemory;
#else
	Application::Get()->UnmapFile( it->second.pMemory, it->second.nSize );
	Application::Get()->FileClose( it->second.hFile );
#endif
	it->second.hFile = (void *)SIRENGINE_INVALID_HANDLE;
}

void CFileCache::AllocateCache( const FileSystem::CFilePath& directory )
{
	size_t i, nCachedBytes;
	FileCacheEntry_t *pCacheEntry;
	CVector<CFilePath> fileList = eastl::move( Application::Get()->ListFiles( directory, false ) );

	SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Verbose, "Allocating file cache in directory \"%s\"", directory.c_str() );

	m_CacheList.reserve( fileList.size() );
	nCachedBytes = 0;

	for ( i = 0; i < fileList.size(); i++ ) {
		pCacheEntry = &m_CacheList.try_emplace( fileList[i] ).first->second;
		pCacheEntry->hFile = (void *)SIRENGINE_INVALID_HANDLE;
	}
}

void CFileCache::MapFile( const FileSystem::CFilePath& filePath, FileCacheEntry_t *pCacheEntry )
{
	SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Verbose, "Caching file \"%s\"...", filePath.c_str() );

	CThreadAutoLock<CThreadMutex> _( m_hLock );
	pCacheEntry->hFile = Application::Get()->FileOpen( filePath, FileMode_ReadOnly );
	if ( (PlatformTypes::file_t)(uintptr_t)pCacheEntry->hFile == SIRENGINE_INVALID_HANDLE ) {
		SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Warning, "Error opening file" );
		memset( pCacheEntry, 0, sizeof( *pCacheEntry ) );
		return;
	}

#if defined(SIRENGINE_NO_FILE_MAPPING)
	pCacheEntry->nSize = Application::Get()->FileLength( pCacheEntry->hFile );
	pCacheEntry->pMemory = new char[ pCacheEntry->nSize ];
	Application::Get()->FileRead( pCacheEntry->pMemory, pCacheEntry->nSize, pCacheEntry->hFile );
	Application::Get()->FileClose( pCacheEntry->hFile );
#else
	pCacheEntry->nSize = Application::Get()->FileLength( pCacheEntry->hFile );
	if ( pCacheEntry->nSize == 0 ) {
		return; // just an empty file
	}
	pCacheEntry->pMemory = Application::Get()->MapFile( pCacheEntry->hFile, &pCacheEntry->nSize );
	if ( pCacheEntry->pMemory == NULL ) {
		SIRENGINE_LOG_LEVEL( FileCache, ELogLevel::Warning, "Error mapping file \"%s\" into memory!", filePath.c_str() );
		Application::Get()->FileClose( pCacheEntry->hFile );
		memset( pCacheEntry, 0, sizeof( *pCacheEntry ) );
		return;
	}
//	Application::Get()->CommitByAddress( pCacheEntry->pMemory, pCacheEntry->nSize );
#endif
}

};
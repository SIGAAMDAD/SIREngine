#ifndef __SIRENGINE_FILECACHE_H__
#define __SIRENGINE_FILECACHE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/Core/Types.h>
#include "FilePath.h"
#include <Engine/Util/CHashMap.h>
#include <Engine/Core/ThreadSystem/Thread.h>

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

		FileCacheEntry_t *GetFile( const CFilePath& filePath );

		void LoadCachedFile( const CFilePath& filePath );
		void ReleaseCachedFile( const CFilePath& filePath );

		SIRENGINE_FORCEINLINE const CHashMap<CFilePath, FileCacheEntry_t>& GetCachedFiles( void ) const
		{ return m_CacheList; }
	private:
		void AllocateCache( const CFilePath& directory );
		void MapFile( const CFilePath& filePath, FileCacheEntry_t *pCacheEntry );

		CHashMap<CFilePath, FileCacheEntry_t> m_CacheList;
		CThreadMutex m_hLock;
	};
};

#endif
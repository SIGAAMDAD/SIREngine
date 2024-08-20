#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "FilePath.h"
#include "SearchPath.h"
#include "FileReader.h"
#include "FileWriter.h"
#include "FileList.h"
#include "FileCache.h"
#include <Engine/Util/CHashMap.h>
#include <Engine/Memory/Allocators/VirtualStackAllocator.h>

class CTagArenaAllocator;

namespace SIREngine::FileSystem {
	class CFileSystem
	{
	public:
		CFileSystem( void );
		~CFileSystem();

		SIRENGINE_FORCEINLINE const CFilePath& GetResourcePath( void ) const
		{ return m_ResourcePath; }
		SIRENGINE_FORCEINLINE const CFilePath& GetConfigPath( void ) const
		{ return m_ConfigPath; }
		SIRENGINE_FORCEINLINE CFileCache *GetFileCache( const CFilePath& filePath )
		{
			for ( auto& it : m_FileCache ) {
				if ( it.second->GetFile( filePath ) ) {
					return it.second;
				}
			}
			return NULL;
		}
		SIRENGINE_FORCEINLINE const CHashMap<CFilePath, CFileCache *>& GetFileCache( void ) const
		{ return m_FileCache; }

		CFileWriter *OpenFileWriter( const CFilePath& filePath );
		CFileReader *OpenFileReader( const CFilePath& filePath );

		void LoadFile( const CFilePath& fileName, uint8_t **pOutBuffer, uint64_t *nOutSize );
		void AddCacheDirectory( const CFilePath& directory );

		CFileList *ListFiles( const CFilePath& directory, const char *pExtension, bool bDirectoryOnly = false );
		const char *BuildSearchPath( const CFilePath& basePath, const CString& fileName ) const;
	private:
		void InitDirectoryCache( void );
		void LoadFileTree( const FileSystem::CFilePath& directory );
		
		void CreateDirectoryTree( const char *pDirectoryPath );

		CFilePath m_CurrentPath;

		CFilePath m_ResourcePath;
		CFilePath m_ConfigPath;

		CHashMap<CFilePath, CFileCache *> m_FileCache;

		CHashMap<CFilePath, CFileList *> m_DirectoryCache;

		static uint64_t nFileSystemTag;
		static uint64_t nDirectoryCacheTag;
		static uint64_t nFileDataTag;
	};
};

extern SIREngine::FileSystem::CFileSystem *g_pFileSystem;

#endif
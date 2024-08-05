#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "FilePath.h"
#include "SearchPath.h"
#include "FileReader.h"
#include "FileWriter.h"

class CTagArenaAllocator;

namespace FileSystem {
    class CDirectory {
        friend class CFileSystem;
    public:
        CDirectory( const CFilePath& basePath )
            : m_BasePath( eastl::move( basePath ) )
        { }
        ~CDirectory()
        { }

        SIRENGINE_FORCEINLINE const CFilePath& GetBasePath( void ) const
        { return m_BasePath; }
        SIRENGINE_FORCEINLINE const CVector<CFilePath>& GetFiles( void ) const
        { return m_FileList; }
    private:
        CFilePath m_BasePath;
        CVector<CFilePath> m_FileList;
    };

    class CFileSystem
    {
    public:
        CFileSystem( void );
        ~CFileSystem();

        SIRENGINE_FORCEINLINE const CFilePath& GetResourcePath( void ) const
        { return m_ResourcePath; }
        SIRENGINE_FORCEINLINE const CFilePath& GetConfigPath( void ) const
        { return m_ConfigPath; }

        CFileWriter *OpenFileWriter( const CFilePath& filePath );

        void LoadFile( const CFilePath& fileName );

        CVector<CFilePath>& ListFiles();
    private:
        void AddSearchPath( const CFilePath& path, const CFilePath& dir );
        void InitDirectoryCache( void );
        void LoadFileTree( CDirectory *pDirectory );

        CFilePath m_CurrentPath;

        CFilePath m_ResourcePath;
        CFilePath m_ConfigPath;

        eastl::unordered_map<CString, CDirectory *> m_DirectoryCache;

        SearchPath_t *m_pSearchPaths;

        static uint64_t nFileSystemTag;
        static uint64_t nDirectoryCacheTag;
        static uint64_t nFileDataTag;
        static CTagArenaAllocator *pArenaAllocator;
    };
};

extern FileSystem::CFileSystem *g_pFileSystem;

#endif
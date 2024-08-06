#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "FilePath.h"
#include "SearchPath.h"
#include "FileReader.h"
#include "FileWriter.h"
#include "FileList.h"

class CTagArenaAllocator;

namespace FileSystem {
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
        CFileReader *OpenFileReader( const CFilePath& filePath );

        void LoadFile( const CFilePath& fileName, CVector<uint8_t>& outBuffer );

        CFileList *ListFiles( const CFilePath& directory, const char *pExtension ) const;
        const char *BuildSearchPath( const CFilePath& basePath, const CString& fileName ) const;
    private:;
        void InitDirectoryCache( void );
        void LoadFileTree( CFileList *pDirectory );

        CFilePath m_CurrentPath;

        CFilePath m_ResourcePath;
        CFilePath m_ConfigPath;

        eastl::unordered_map<CFilePath, CFileList *> m_DirectoryCache;

        SearchPath_t *m_pSearchPaths;

        static uint64_t nFileSystemTag;
        static uint64_t nDirectoryCacheTag;
        static uint64_t nFileDataTag;
        static CTagArenaAllocator *pArenaAllocator;
    };
};

extern FileSystem::CFileSystem *g_pFileSystem;

#endif
#ifndef __SIRENGINE_FILESYSTEM_H__
#define __SIRENGINE_FILESYSTEM_H__

#pragma once

#include "FilePath.h"
#include "SearchPath.h"
#include "FileReader.h"
#include "FileWriter.h"

namespace FileSystem {
    class CDirectory {
        friend class CFileSystem;
    public:
        CDirectory( void )
        { }
        ~CDirectory()
        { }

        SIRENGINE_FORCEINLINE const CFilePath& GetBasePath( void ) const
        { return m_BasePath; }
        SIRENGINE_FORCEINLINE const CVector<CDirectory *>& GetSubDirs( void ) const
        { return m_SubDirs; }
        SIRENGINE_FORCEINLINE const CVector<CFilePath>& GetFiles( void ) const
        { return m_FileList; }
    private:
        CFilePath m_BasePath;
        CVector<CDirectory *> m_SubDirs;
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

        CFilePath m_ResourcePath;
        CFilePath m_ConfigPath;

        eastl::map<CString, CDirectory> m_DirectoryCache;

        SearchPath_t *m_pSearchPaths;
    };
};

#endif
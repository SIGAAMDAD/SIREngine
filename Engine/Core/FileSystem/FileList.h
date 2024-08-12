#ifndef __SIRENGINE_FILELIST_H__
#define __SIRENGINE_FILELIST_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Util/CVector.h>
#include "FilePath.h"

namespace SIREngine::FileSystem {
    class CFileList
    {
        friend class CFileSystem;
    public:
        CFileList( const CFilePath& filePath )
            : m_BasePath( filePath )
        { }
        CFileList( const CFilePath& filePath, const CVector<CFilePath>& fileList )
            : m_BasePath( eastl::move( filePath ) ), m_List( eastl::move( fileList ) )
        { }
        CFileList( void )
        { }
        ~CFileList()
        { }

        SIRENGINE_FORCEINLINE const CFilePath& GetPath( void ) const
        { return m_BasePath; }
        SIRENGINE_FORCEINLINE const CVector<CFilePath>& GetList( void ) const
        { return m_List; }
    private:
        CFilePath m_BasePath;
        CVector<CFilePath> m_List;
    };
};

#endif
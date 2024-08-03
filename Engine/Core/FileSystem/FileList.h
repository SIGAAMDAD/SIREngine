#ifndef __SIRENGINE_FILELIST_H__
#define __SIRENGINE_FILELIST_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include "FilePath.h"

namespace FileSystem {
    class CFileList
    {
    public:
        CFileList( void );
        ~CFileList();

        SIRENGINE_FORCEINLINE const CVector<CFilePath>& GetList( void ) const
        { return m_List; }
    private:
        CFilePath m_BasePath;
        CVector<CFilePath> m_List;
    };
};

#endif
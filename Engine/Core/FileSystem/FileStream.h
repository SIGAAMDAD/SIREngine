#ifndef __SIRENGINE_FILESTREAM_H__
#define __SIRENGINE_FILESTREAM_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Util/CString.h>
#include "FilePath.h"

namespace SIREngine::FileSystem {
    class IFileStream
    {
    public:
        IFileStream( void )
        { }
        virtual ~IFileStream()
        { }

        inline const CFilePath& GetPath( void ) const
        { return m_FilePath; }

        virtual bool Open( const CFilePath& filePath ) = 0;
        virtual void Close( void ) = 0;
        virtual bool IsOpen( void ) const = 0;

        virtual size_t GetPosition( void ) const = 0;
        virtual size_t GetLength( void ) const = 0;
    protected:
        CFilePath m_FilePath;
        FILE *m_hFileHandle;
    private:
        IFileStream( IFileStream&& );
    };
};

#endif
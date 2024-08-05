#ifndef __SIRENGINE_FILEREADER_H__
#define __SIRENGINE_FILEREADER_H__

#pragma once

#include "FilePath.h"
#include "FileStream.h"

namespace FileSystem {
    class CFileReader
    {
    public:
        CFileReader( void )
            : m_hFileHandle( NULL )
        { }
        CFileReader( const CFilePath& filePath )
            : m_FilePath( filePath )
        { Open( filePath ); }
        ~CFileReader()
        { Close(); }

        bool Open( const CFilePath& filePath );
        void Close( void );
        bool IsOpen( void ) const;

        size_t Read( void *pBuffer, size_t nBytes );
        size_t GetPosition( void ) const;
        size_t GetLength( void ) const;
    private:
        CFilePath m_FilePath;
        FILE *m_hFileHandle;
    };
};

#endif
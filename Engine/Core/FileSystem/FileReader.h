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

    inline bool CFileReader::Open( const CFilePath& filePath )
    {
        m_hFileHandle = g_pApplication->OpenFile( filePath, "rb" );
        return m_hFileHandle != NULL;
    }

    inline void CFileReader::Close( void )
    {
        if ( m_hFileHandle != NULL ) {
            fclose( m_hFileHandle );
            m_hFileHandle = NULL;
        }
    }

    inline bool CFileReader::IsOpen( void ) const
    {
        return m_hFileHandle != NULL;
    }

    inline size_t CFileReader::Read( void *pBuffer, size_t nBytes )
    {
        return fread( pBuffer, nBytes, 1, m_hFileHandle );
    }

    inline size_t CFileReader::GetPosition( void ) const
    {
        return ftell( m_hFileHandle );
    }

    inline size_t CFileReader::GetLength( void ) const
    {
        FILE *hFile = const_cast<FILE *>( m_hFileHandle );

        if ( hFile ) {
            size_t nLength = 0;
            size_t nPosition = GetPosition();

            fseek( m_hFileHandle, 0L, SEEK_END );
            nLength = ftell( m_hFileHandle );
            fseek( m_hFileHandle, nPosition, SEEK_SET );

            return nLength;
        } else {

        }

        return 0;
    }
};

#endif
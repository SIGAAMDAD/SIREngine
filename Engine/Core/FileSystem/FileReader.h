#ifndef __SIRENGINE_FILEREADER_H__
#define __SIRENGINE_FILEREADER_H__

#pragma once

#include "FileStream.h"

class CFileReader : public IFileStream
{
public:
    CFileReader( void )
        : m_hFileHandle( NULL )
    { }
    CFileReader( const CString& filePath )
    { Open( filePath ); }
    virtual ~CFileReader() override
    { Close(); }

    virtual bool Open( const CString& filePath ) override;
    virtual void Close( void ) override;
    virtual bool IsOpen( void ) const override;

    size_t Read( void *pBuffer, size_t nBytes );
    virtual size_t GetPosition( void ) const override;
    virtual size_t GetLength( void ) const override;
private:
    CString m_FilePath;
    FILE *m_hFileHandle;
};

inline bool CFileReader::Open( const CString& filePath )
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
}

#endif
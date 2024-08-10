#ifndef __SIRENGINE_FILEWRITER_H__
#define __SIRENGINE_FILEWRITER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/Logging/Logger.h>
#include "FileStream.h"

namespace FileSystem {
    class CFileWriter : public IFileStream
    {
    public:
        CFileWriter( void )
        { }
        CFileWriter( CFileWriter& other )
        {
            m_FilePath = eastl::move( other.m_FilePath );
            m_hFileHandle = other.m_hFileHandle;
        }
        CFileWriter( const CFilePath& filePath )
        { Open( filePath ); }
        virtual ~CFileWriter() override
        { Close(); }
        
        size_t Write( const void *pBuffer, size_t nBytes );
        void Flush( void );

        void Printf( const char *fmt, ... ) SIRENGINE_ATTRIBUTE(format(printf, 2, 3));

        virtual bool Open( const CFilePath& filePath ) override;
        virtual void Close( void ) override
        {
            if ( m_hFileHandle != NULL ) {
                fclose( m_hFileHandle );
                m_hFileHandle = NULL;
            }
        }
        virtual bool IsOpen( void ) const override
        { return m_hFileHandle != NULL; }
        virtual size_t GetPosition( void ) const override
        { return ftell( m_hFileHandle ); }
        virtual size_t GetLength( void ) const override
        {
            const size_t pos = GetPosition();
            
            fseek( m_hFileHandle, 0L, SEEK_END );
            const size_t len = ftell( m_hFileHandle );
            fseek( m_hFileHandle, pos, SEEK_SET );

            return len;
        }
    };
};

#endif
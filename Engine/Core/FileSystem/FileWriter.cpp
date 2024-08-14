#include "FileWriter.h"
#include <Engine/Core/SIREngine.h>
#include <Engine/Core/Util.h>
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>

namespace SIREngine::FileSystem {

bool CFileWriter::Open( const CFilePath& filePath )
{
    SIRENGINE_LOG( "Opening file \"%s\"...", filePath.c_str() );
    m_hFileHandle = Application::Get()->OpenFile( filePath.c_str(), "w" );
    return m_hFileHandle != NULL;
}

void CFileWriter::Close( void )
{
    if ( m_hFileHandle != NULL ) {
        fclose( m_hFileHandle );
        m_hFileHandle = NULL;
    }
}

size_t CFileWriter::Write( const void *pBuffer, size_t nBytes )
{
    const uint8_t *buf = (const uint8_t *)pBuffer;
    size_t nRemaining, nBlockSize, nWritten;
    int tries;

    if ( m_hFileHandle == NULL ) {
        return 0;
    }
    if ( !pBuffer || !nBytes ) {
        return 0;
    }

    nRemaining = nBytes;
    tries = 0;

    while ( nRemaining ) {
        nBlockSize = nRemaining;
        nWritten = fwrite( buf, 1, nBlockSize, m_hFileHandle );
        if ( nWritten == 0 ) {
            if ( !tries ) {
                tries = 1;
            } else {
                SIRENGINE_WARNING( "CFileWriter::Write: wrote 0 bytes" );
                return 0;
            }
        }

        buf += nWritten;
        nRemaining -= nWritten;
    }

    return nBytes;
}

void SIRENGINE_ATTRIBUTE(format(printf, 2, 3)) CFileWriter::Printf( const char *fmt, ... )
{
    va_list argptr;
    char msg[8192];
    int length;

    va_start( argptr, fmt );
    length = SIREngine_Vsnprintf( msg, sizeof( msg ) - 1, fmt, argptr );
    va_end( argptr );

    Write( msg, length );
}

void CFileWriter::Flush( void )
{
    fflush( m_hFileHandle );
}

size_t CFileWriter::GetLength( void ) const
{
    const size_t pos = GetPosition();
        
    fseek( m_hFileHandle, 0L, SEEK_END );
    const size_t len = ftell( m_hFileHandle );
    fseek( m_hFileHandle, pos, SEEK_SET );

    return len;
}

};
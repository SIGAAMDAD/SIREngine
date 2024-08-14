#include "FileReader.h"
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>
#include <Engine/Core/Logging/Logger.h>

namespace SIREngine::FileSystem {

bool CFileReader::Open( const CFilePath& filePath )
{
	m_hFileHandle = Application::Get()->OpenFile( filePath, "rb" );
    return m_hFileHandle != NULL;
}

void CFileReader::Close( void )
{
	if ( m_hFileHandle != NULL ) {
		fclose( m_hFileHandle );
		m_hFileHandle = NULL;
	}
}

bool CFileReader::IsOpen( void ) const
{
	return m_hFileHandle != NULL;
}

size_t CFileReader::GetPosition( void ) const
{
	if ( !m_hFileHandle ) {
        SIRENGINE_WARNING( "CFileReader::GetPosition: NULL handle" );
        return 0;
    }
    return ftell( m_hFileHandle );
}

size_t CFileReader::GetLength( void ) const
{
	if ( !m_hFileHandle ) {
        SIRENGINE_WARNING( "CFileReader::GetLength: NULL handle" );
        return 0;
    }
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

size_t CFileReader::Read( void *pBuffer, size_t nBytes )
{

}

};
#include "IniSerializer.h"
#include <errno.h>

bool CIniSerializer::Save( const FileSystem::CFilePath& filePath )
{
    m_FilePath = filePath;
    FileSystem::CFileWriter *pFile = g_pFileSystem->OpenFileWriter( filePath );
    const char *str;

    if ( !pFile ) {
        SIRENGINE_WARNING( "Error opening write-only .ini file \"%s\": %s", filePath.c_str(), strerror( errno ) );
        return false;
    }

    SIRENGINE_LOG( "Writing %lu sections...", m_Values.size() );
    for ( const auto& section : m_Values ) {
        pFile->Printf( "[%s]\n", section.first.c_str() );
        for ( const auto& value : section.second ) {
            SIRENGINE_LOG( "\"%s\" = \"%s\"\n", value.first.c_str(), value.second.c_str() );
            pFile->Printf( "\"%s\" = \"%s\"\n", value.first.c_str(), value.second.c_str() );
        }
        pFile->Printf( "\n" );
    }

    delete pFile;

    return true;
}
#include "IniSerializer.h"
#include <Engine/Core/FileSystem/FileWriter.h>
#include <Engine/Core/FileSystem/FileSystem.h>
#include <errno.h>

using namespace SIREngine::Serialization;
using namespace SIREngine;

bool CIniSerializer::Save( const FileSystem::CFilePath& filePath )
{
    m_FilePath = filePath;
    FileSystem::CFileWriter *pFile = g_pFileSystem->OpenFileWriter( filePath );
    const char *str;

    if ( !pFile ) {
        SIRENGINE_WARNING( "Error opening write-only .ini file \"%s\": %s", filePath.c_str(), strerror( errno ) );
        return false;
    }

    SIRENGINE_NOTIFICATION( "Writing %lu sections...", m_Values.size() );
    for ( const auto& section : m_Values ) {
        pFile->Printf( "[%s]\n", section.first.c_str() );
        for ( const auto& value : section.second ) {
            pFile->Printf( "%s=%s\n", value.first.c_str(), value.second.c_str() );
        }
        pFile->Printf( "\n" );
    }

    delete pFile;

    return true;
}
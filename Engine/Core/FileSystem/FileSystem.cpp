#include "FileSystem.h"

using namespace FileSystem;

CFileSystem::CFileSystem( void )
{
}

CFileSystem::~CFileSystem()
{
}

void CFileSystem::InitDirectoryCache( void )
{
    
}

SIRENGINE_EXPORT_DEMANGLE void CFileSystem::AddSearchPath( const CFilePath& path, const CFilePath& dir )
{
    SearchPath_t *pSearchPath;

    for ( pSearchPath = m_pSearchPaths; pSearchPath; pSearchPath = pSearchPath->pNext ) {
        if ( pSearchPath->directory.path == path && pSearchPath->directory.gamedir == dir ) {
            return; // we've already got this one
        }
    }
}

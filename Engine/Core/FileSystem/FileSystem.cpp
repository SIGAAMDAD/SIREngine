#include "FileSystem.h"
#include <Engine/Memory/Backend/TagArenaAllocator.h>

using namespace FileSystem;

CFileSystem::CFileSystem( void )
{
    InitDirectoryCache();
}

CFileSystem::~CFileSystem()
{
}

void CFileSystem::LoadFileTree( CDirectory *pDirectory )
{
    SIRENGINE_LOG( "Loading directory tree \"%s\"", pDirectory->GetBasePath().c_str() );

    pDirectory->m_FileList = eastl::move( g_pApplication->ListFiles( pDirectory->GetBasePath() ) );

    const CVector<CFilePath> subDirs = eastl::move( g_pApplication->ListFiles( pDirectory->GetBasePath(), true ) );
    for ( const auto& it : subDirs ) {
        // recurse
        LoadFileTree( new CDirectory( it ) );
    }
}

void CFileSystem::InitDirectoryCache( void )
{
    SIRENGINE_LOG( "Initializing Directory Cache..." );
    SIRENGINE_LOG( "GameDirectory: %s", g_pApplication->GetGamePath().c_str() );

    m_CurrentPath = g_pApplication->GetGamePath();

    const CVector<CFilePath> dirList = eastl::move( g_pApplication->ListFiles( g_pApplication->GetGamePath(), true ) );
    m_DirectoryCache.reserve( dirList.size() );

    SIRENGINE_LOG( "Got %lu Directories.", dirList.size() );

    for ( const auto& it : dirList ) {
        LoadFileTree( new CDirectory( it ) );
    }
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

bool CFileReader::Open( const CFilePath& filePath )
{
    m_hFileHandle = g_pApplication->OpenFile( filePath, "rb" );
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

size_t CFileReader::Read( void *pBuffer, size_t nBytes )
{
    return fread( pBuffer, nBytes, 1, m_hFileHandle );
}

size_t CFileReader::GetPosition( void ) const
{
    return ftell( m_hFileHandle );
}

size_t CFileReader::GetLength( void ) const
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

size_t CFileWriter::Write( const void *pBuffer, size_t nBytes )
{
    const uint8_t *buf = (const uint8_t *)pBuffer;
    size_t nRemaining, nBlockSize, nWritten;
    int tries;

    if ( m_hFile == NULL ) {
        return 0;
    }
    if ( !pBuffer || !nBytes ) {
        return 0;
    }

    nRemaining = nBytes;
    tries = 0;

    while ( nRemaining ) {
        nBlockSize = nRemaining;
        nWritten = fwrite( buf, nBlockSize, 1, m_hFile );
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

void CFileWriter::Flush( void )
{
    fflush( m_hFile );
}
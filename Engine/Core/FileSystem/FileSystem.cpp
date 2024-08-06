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

void CFileSystem::LoadFileTree( CFileList *pDirectory )
{
    SIRENGINE_LOG( "Loading directory tree \"%s\"", pDirectory->GetPath().c_str() );

    pDirectory->m_List = eastl::move( g_pApplication->ListFiles( pDirectory->GetPath(), true ) );
    m_DirectoryCache.try_emplace( pDirectory->GetPath(), pDirectory );

    const CVector<CFilePath> subDirs = eastl::move( g_pApplication->ListFiles( pDirectory->GetPath(), true ) );
    m_DirectoryCache.reserve( subDirs.size() );

    for ( const auto& it : subDirs ) {
        // recurse
        LoadFileTree( new CFileList( it ) );
    }
}

void CFileSystem::InitDirectoryCache( void )
{
    const char *pszSearchPath;

    SIRENGINE_LOG( "Initializing Directory Cache..." );
    SIRENGINE_LOG( "GameDirectory: %s", g_pApplication->GetGamePath().c_str() );

    m_CurrentPath = g_pApplication->GetGamePath();

    const CVector<CFilePath> resourceDirList = eastl::move( g_pApplication->ListFiles( BuildSearchPath( "Resources", "" ), true ) );
    const CVector<CFilePath> configDirList = eastl::move( g_pApplication->ListFiles( BuildSearchPath( "Config", "" ), true ) );

    m_DirectoryCache.reserve( resourceDirList.size() + configDirList.size() );

    SIRENGINE_LOG( "Got %lu Directories.", resourceDirList.size() + configDirList.size() );

    for ( const auto& it : resourceDirList ) {
        LoadFileTree( new CFileList( it ) );
    }
    for ( const auto& it : configDirList ) {
        LoadFileTree( new CFileList( it ) );
    }
}

CFileWriter *CFileSystem::OpenFileWriter( const CFilePath& filePath )
{
    CFileWriter *hFile;
    const char *pSearchPath;

    pSearchPath = BuildSearchPath( m_CurrentPath, filePath.c_str() );

    SIRENGINE_LOG( "Attempting file open at \"%s\" for CFileWriter", pSearchPath );

    hFile = new CFileWriter( pSearchPath );
    if ( hFile->IsOpen() ) {
        return hFile;
    }

    delete hFile;
    return NULL;
}

CFileReader *CFileSystem::OpenFileReader( const CFilePath& filePath )
{
    CFileReader hFile;
    const char *pSearchPath;

    for ( const auto& it : m_DirectoryCache ) {
        pSearchPath = BuildSearchPath( it.second->GetPath(), filePath.c_str() );

        SIRENGINE_LOG( "Attempting file open at \"%s\" for CFileReader", pSearchPath );

        if ( hFile.Open( pSearchPath ) ) {
            break;
        }
    }

    if ( hFile.IsOpen() ) {
        return new CFileReader( hFile );
    }
    return NULL;
}

CFileList *CFileSystem::ListFiles( const CFilePath& directory, const char *pExtension ) const
{
    CFileList *fileList;
    uint64_t nFiles;
    const char *path;

    path = BuildSearchPath( directory, "" );

    const auto& it = m_DirectoryCache.find( path );
    if ( it == m_DirectoryCache.cend() ) {
        SIRENGINE_WARNING( "Invalid search directory \"%s\"", path );
        return NULL;
    }

    nFiles = 0;
    for ( const auto& file : it->second->GetList() ) {
        if ( SIREngine_stricmp( CFilePath::GetExtension( file.c_str() ).c_str(), pExtension ) == 0 ) {
            nFiles++;
        }
    }

    fileList = new CFileList( directory );
    fileList->m_List.reserve( nFiles );
    for ( const auto& file : it->second->GetList() ) {
        if ( SIREngine_stricmp( CFilePath::GetExtension( file.c_str() ).c_str(), pExtension ) == 0 ) {
            fileList->m_List.emplace_back( file );
        }
    }

    return fileList;
}

const char *CFileSystem::BuildSearchPath( const CFilePath& basePath, const CString& fileName ) const
{
    static char szSearchPath[4][ SIRENGINE_MAX_OSPATH*2+1 ];
    static int toggleBit = 0;
    char *pString;

    toggleBit++;
    pString = szSearchPath[ toggleBit % 4 ];

    if ( fileName.size() ) {
        SIREngine_snprintf( pString, sizeof( *szSearchPath ) - 1, "%s/%s", basePath.c_str(), fileName.c_str() );
    } else {
        SIREngine_snprintf( pString, sizeof( *szSearchPath ) - 1, "%s", basePath.c_str() );
    }

    return pString;
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

bool CFileWriter::Open( const CFilePath& filePath )
{
    SIRENGINE_LOG( "Opening file \"%s\"...", filePath.c_str() );
    m_hFileHandle = fopen( filePath.c_str(), "w" );
    return m_hFileHandle != NULL;
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
#include "FileSystem.h"
#include <Engine/Memory/Backend/TagArenaAllocator.h>
#include "MemoryFile.h"

using namespace SIREngine::Application;

namespace SIREngine::FileSystem {

SIRENGINE_DEFINE_LOG_CATEGORY( FileSystem, ELogLevel::Info );

CFileSystem::CFileSystem( void )
{
	InitDirectoryCache();
}

CFileSystem::~CFileSystem()
{
}

void CFileSystem::AddCacheDirectory( const CFilePath& directory )
{
	if ( m_FileCache.find( directory ) != m_FileCache.end() ) {
		SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Warning, "CFileSystem::AddCacheDirectory: directory \"%s\" already in cache.",
			directory.c_str() );
		return;
	}
	m_FileCache.try_emplace( directory, new CFileCache( directory ) );
}

void CFileSystem::LoadFileTree( const FileSystem::CFilePath& directory )
{
	CFileList *pDirectory;

	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Info, "Loading directory tree \"%s\"", directory.c_str() );

	const CVector<CFilePath> fileList = eastl::move( g_pApplication->ListFiles( directory, false ) );

	pDirectory = new CFileList( directory );
	pDirectory->m_List = eastl::move( fileList );
	m_DirectoryCache.try_emplace( directory, pDirectory );

	const CVector<CFilePath> subDirs = eastl::move( g_pApplication->ListFiles( directory, true ) );
	pDirectory->m_SubDirs = eastl::move( subDirs );
	m_DirectoryCache.reserve( pDirectory->m_SubDirs.size() );

	for ( const auto& it : pDirectory->m_SubDirs ) {
		// recurse
		LoadFileTree( it );
	}
}

void CFileSystem::InitDirectoryCache( void )
{
	const char *pszSearchPath;

	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Info, "Initializing Directory Cache..." );
	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Info, "GameDirectory: %s", g_pApplication->GetGamePath().c_str() );

	m_CurrentPath = g_pApplication->GetGamePath();

	const CVector<CFilePath> dirList = eastl::move( g_pApplication->ListFiles( g_pApplication->GetGamePath(), true ) );

	m_DirectoryCache.reserve( dirList.size() );

	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Info, "Got %lu Directories.", dirList.size() );

	for ( const auto& it : dirList ) {
		LoadFileTree( it );
	}
}

void CFileSystem::CreateDirectoryTree( const char *pDirectoryPath )
{
	char szPath[ SIRENGINE_MAX_OSPATH*2+1 ];
	char *ofs;

	SIREngine_strncpyz( szPath, pDirectoryPath, sizeof( szPath ) );

	for ( ofs = szPath + 1; *ofs; ofs++ ) {
		if ( *ofs == SIRENGINE_PATH_SEPERATOR ) {
			// create the directory
			*ofs = '\0';
			Application::Get()->CreateDirectory( szPath );
			*ofs = SIRENGINE_PATH_SEPERATOR;
		}
	}
}

CFileWriter *CFileSystem::OpenFileWriter( const CFilePath& filePath )
{
	CFileWriter *hFile;
	const char *pSearchPath;

	pSearchPath = BuildSearchPath( m_CurrentPath, filePath.c_str() );

	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Verbose, "Attempting file open at \"%s\" for CFileWriter", pSearchPath );

	hFile = new CFileWriter( pSearchPath );
	if ( hFile->IsOpen() ) {
		return hFile;
	} else {
		CreateDirectoryTree( pSearchPath );

		hFile->Open( pSearchPath );
		if ( hFile->IsOpen() ) {
			return hFile;
		}
	}

	delete hFile;
	return NULL;
}

void CFileSystem::LoadFile( const CFilePath& filePath, uint8_t **pOutBuffer, uint64_t *nSize )
{
	FileCacheEntry_t *pCacheEntry;

	SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Info, "Loading CacheFile \"%s\"...", filePath.c_str() );

	for ( const auto& it : m_FileCache ) {
		pCacheEntry = it.second->GetFile( filePath );
		if ( pCacheEntry ) {
			break;
		}
	}

	if ( pCacheEntry ) {
		*pOutBuffer = (uint8_t *)pCacheEntry->pMemory;
		*nSize = pCacheEntry->nSize;
	}
	else {
		*pOutBuffer = NULL;
		*nSize = 0;
	}
}

CFileReader *CFileSystem::OpenFileReader( const CFilePath& filePath )
{
	CFileReader hFile;
	const char *pSearchPath;

	for ( const auto& it : m_DirectoryCache ) {
		pSearchPath = BuildSearchPath( it.second->GetPath(), filePath.c_str() );

		SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Verbose, "Attempting file open at \"%s\" for CFileReader", pSearchPath );

		if ( hFile.Open( pSearchPath ) ) {
			break;
		}
	}

	if ( hFile.IsOpen() ) {
		return new CFileReader( hFile );
	}
	return NULL;
}

CFileList *CFileSystem::ListFiles( const CFilePath& directory, const char *pExtension, bool bDirectoryOnly )
{
	CFileList *fileList;
	uint64_t nFiles;
	const char *path;

	path = BuildSearchPath( directory, "" );

	const auto& it = m_DirectoryCache.find( path );
	if ( it == m_DirectoryCache.cend() ) {
		LoadFileTree( directory );
		
		const auto& it = m_DirectoryCache.find( path );
		if ( it == m_DirectoryCache.cend() ) {
			SIRENGINE_LOG_LEVEL( FileSystem, ELogLevel::Warning, "Invalid search directory \"%s\"", path );
		}
		return NULL;
	}

	if ( !pExtension ) {
		pExtension = "";
	}

	if ( !bDirectoryOnly ) {
		nFiles = 0;
		for ( const auto& file : it->second->GetList() ) {
			if ( *pExtension != '\0' ) {
				if ( SIREngine_stricmp( CFilePath::GetExtension( file.c_str() ).c_str(), pExtension ) == 0 ) {
					nFiles++;
				}
			} else {
				nFiles++;
			}
		}
	}
	else {
		nFiles = it->second->GetSubDirs().size();
	}

	fileList = new CFileList( directory );
	if ( bDirectoryOnly ) {
		fileList->m_SubDirs = it->second->GetSubDirs();
	}
	else {
		fileList->m_List.reserve( nFiles );
		for ( const auto& file : it->second->GetList() ) {
			if ( *pExtension != '\0' ) {
				if ( SIREngine_stricmp( CFilePath::GetExtension( file.c_str() ).c_str(), pExtension ) == 0 ) {
					fileList->m_List.emplace_back( file );
				}
			} else {
				fileList->m_List.emplace_back( file );
			}
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

};
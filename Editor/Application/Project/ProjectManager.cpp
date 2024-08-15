#include "ProjectManager.h"
#include <Engine/Core/FileSystem/MemoryFile.h>

namespace Valden {

eastl::unique_ptr<CProjectManager> CProjectManager::g_pProjectManager;
SIRENGINE_DEFINE_LOG_CATEGORY( ProjectManager, ELogLevel::Type::Info );

CProjectManager::CProjectManager( void )
{
}

CProjectManager::~CProjectManager()
{
}

void CProjectManager::SaveCache( void )
{
	FileSystem::CFileWriter *hFile = g_pFileSystem->OpenFileWriter( "Valden/Projects/Cached.json" );

	SIRENGINE_LOG( "Saving ProjectCache..." );

	if ( !hFile ) {
		SIRENGINE_WARNING( "Error creating cache file." );
		return;
	}

	hFile->Printf(
		"{\n"
		"\t\"Projects\": [\n"
	);
	for ( auto it = g_pProjectManager->m_ProjectCache.begin(); it != g_pProjectManager->m_ProjectCache.end(); it++ ) {
		if ( it == g_pProjectManager->m_ProjectCache.end() - 1 ) {
			hFile->Printf( "\t\t\"%s\"\n", it->get()->GetName().c_str() );
		} else {
			hFile->Printf( "\t\t\"%s\",\n", it->get()->GetName().c_str() );
		}
		it->get()->Save();
	}
	hFile->Printf(
		"\t]\n"
		"}"
	);

	delete hFile;
}

void CProjectManager::Init( void )
{
	g_pProjectManager = eastl::make_unique<CProjectManager>();

	SIRENGINE_LOG_LEVEL( ProjectManager, ELogLevel::Info, "Loading ProjectCache..." );

	g_pFileSystem->AddCacheDirectory( "Valden/Projects" );

	FileSystem::CMemoryFile file( "Valden/Projects/Cached.json" );
	g_pProjectManager->LoadProjectCache( (const char *)file.GetBuffer(), file.GetSize() );
}

bool CProjectManager::IsProjectLoaded( const CString& projectName ) const
{
	for ( const auto& it : m_ProjectCache ) {
		if ( it->GetName() == projectName ) {
			return true;
		}
	}
	return false;
}

bool CProjectManager::Load( const CString& projectName )
{
	if ( IsProjectLoaded( projectName ) ) {
		return true;
	}

	return true;
}

bool CProjectManager::Load( const FileSystem::CFilePath& directory )
{
	const CString projectName = directory.GetFileName();
	if ( IsProjectLoaded( projectName ) ) {
		return true;
	}

	SIRENGINE_LOG_LEVEL( ProjectManager, ELogLevel::Info, "Loading project from \"%s\"...", directory.c_str() );
	FileSystem::CFilePath path = directory;
	const size_t pos = path.find_last_of( SIRENGINE_PATH_SEPERATOR );
	path[ pos ] = '\0';

	g_pFileSystem->AddCacheDirectory( path );
	eastl::shared_ptr<CProjectData>& project = m_ProjectCache.emplace_back( eastl::make_shared<CProjectData>() );
	project->Load( projectName );
	
	m_pCurrentProject = project;

	return true;
}

void CProjectManager::Create( const CString& projectName )
{
	eastl::shared_ptr<CProjectData>& project = m_ProjectCache.emplace_back( eastl::make_shared<CProjectData>() );

	SIRENGINE_LOG_LEVEL( ProjectManager, ELogLevel::Info, "Created project \"%s\".", projectName.c_str() );
	project->SetName( projectName );
	project->Save();
	project->InitDirectoryStructure();
	
	m_pCurrentProject = project;
}

};
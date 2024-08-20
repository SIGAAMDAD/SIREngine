#include "ScriptObject.h"
#include "../Project/ProjectManager.h"

namespace Valden {

SIRENGINE_DECLARE_LOG_CATEGORY( ScriptCompiler, ELogLevel::Info );

CScriptObject::CScriptObject( const FileSystem::CFilePath& scriptPath )
	: ISceneObject( scriptPath.GetFileName() ), m_FilePath( scriptPath )
{
	FileSystem::CMemoryFile file( scriptPath );
	if ( file.GetSize() ) {
		// get the object's name
		FileSystem::CFilePath tmp = scriptPath;
		tmp.StripExtension();
		m_Name = tmp.GetFileName();

		SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "Created ScriptClass \"%s\" with preloaded class file.", m_Name.c_str() );

		SIREngine_strncpyz( m_szDisplayName, scriptPath.GetFileName().c_str(), sizeof( m_szDisplayName ) );
		m_CodeFile.SetText( (const char *)file.GetBuffer() );
	}
}

CScriptObject::CScriptObject( void )
	: ISceneObject( "New Class" )
{
	memset( m_szDisplayName, 0, sizeof( m_szDisplayName ) );
	strcpy( m_szDisplayName, "New Class" );
	m_Name = m_szDisplayName;
}

CScriptObject::~CScriptObject()
{
}

void CScriptObject::Save( void )
{		
	SIRENGINE_LOG( "Saving ScriptClass \"%s\"...", m_Name.c_str() );

	FileSystem::CFileWriter *hFile;
	if ( m_FilePath.size() == 0 ) {
		// newly created class, save to default Valden/Projects/[Project Name]/Scripts/Classes/... path
		hFile = g_pFileSystem->OpenFileWriter( CProjectManager::Get()->BuildProjectPath( "Scripts/Classes", m_Name.c_str() ) );
	} else {
		hFile = g_pFileSystem->OpenFileWriter( m_FilePath.c_str() );
	}
	if ( !hFile ) {
		SIRENGINE_WARNING( "Error opening or creating ScriptClass file for \"%s\"!", m_Name.c_str() );
		return;
	}

	const CString&& data = eastl::move( m_CodeFile.GetText() );
	hFile->Write( data.c_str(), data.size() );
	delete hFile;
}

};
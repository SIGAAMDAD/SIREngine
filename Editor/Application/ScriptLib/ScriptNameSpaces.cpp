#define SIRENGINE_NEW_AND_DELETE_OVERRIDE
#include "ScriptCompiler.h"
#include <boost/filesystem.hpp>

namespace Valden {

SIRENGINE_DECLARE_LOG_CATEGORY( ScriptCompiler, ELogLevel::Info );

void CScriptCompiler::LoadSubDirs( FileSystem::CFileList *pSubDirs )
{
	const char *str;
	char *ptr;
	char szNameSpace[2048];
	CHashMap<CString, CScriptNameSpace *> nameSpaces;

	for ( const auto& it : pSubDirs->GetSubDirs() ) {
		FileSystem::CFileList *pFiles = g_pFileSystem->ListFiles( it.c_str(), "", false );

		boost::filesystem::path path = it.c_str();

		path.remove_filename();
		const char *pNameSpace = strstr( path.string().c_str(), "Scripts/Classes" );
		if ( nameSpaces.find( pNameSpace ) == nameSpaces.end() ) {
			SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "- Loaded ScriptNamespace \"%s\"", pNameSpace );
			CScriptNameSpace *pObject = new ( Mem_Alloc( sizeof( CScriptNameSpace ) ) ) CScriptNameSpace( strrchr( pNameSpace, '/' ) + 1 );
			CSceneView::Get()->PushObject( pObject );
			nameSpaces.try_emplace( pNameSpace, pObject );
		}
		for ( const auto& it : boost::filesystem::path( pNameSpace ) ) {
			auto parent = nameSpaces.find( it.parent_path().string().c_str() );
			if ( parent != nameSpaces.end() ) {
				parent->second->AddChildNameSpace( nameSpaces.find( strrchr( it.string().c_str(), '/' ) + 1 )->second );
			}
		}

		if ( pFiles ) {
			m_Objects.reserve( pFiles->GetList().size() );
			for ( const auto& file : pFiles->GetList() ) {
				SIRENGINE_LOG_LEVEL( ScriptCompiler, ELogLevel::Info, "- Loaded ScriptClass file \"%s\"", file.c_str() );
				CScriptObject *pObject = new ( Mem_Alloc( sizeof( CScriptObject ) ) ) CScriptObject( file );
				CSceneView::Get()->PushObject( Cast<ISceneObject>( pObject ) );
				pObject->SetNameSpace( nameSpaces[ pNameSpace ] );
				nameSpaces[ pNameSpace ]->AddClass( pObject );
			}
			Mem_Free( pFiles );
		}
		pFiles = g_pFileSystem->ListFiles( it.c_str(), "", true );
		if ( pFiles ) {
			LoadSubDirs( pFiles );
			Mem_Free( pFiles );
		}
	}
}

};
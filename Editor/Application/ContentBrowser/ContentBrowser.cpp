#include "ContentBrowser.h"
#include "../Project/ProjectManager.h"
#include "FileView.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/RenderLib/Backend/OpenGL/GLCommon.h>
#include <Engine/Core/ResourceManager.h>

namespace Valden::ContentBrowser {

extern CVar<float> ThumbnailSize;

SIRENGINE_DEFINE_LOG_CATEGORY( ContentBrowser, ELogLevel::Info );
CContentBrowser CContentBrowser::g_ContentBrowser;

void CContentBrowser::Init( void )
{
	CEditorApplication::Get().AddWidget( &g_ContentBrowser );

	g_ContentBrowser.m_Windows.emplace_back(
		eastl::make_unique<CBrowserInstance>(
			SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s", CProjectManager::Get()->GetProject()->GetName().c_str() )
		)
	);
	
	g_ContentBrowser.m_Windows.back()->AddSubDir( g_ContentBrowser.m_Windows.back()->GetBaseDir(), "Assets" );
	FileView_t *AssetDir = g_ContentBrowser.m_Windows.back()->GetDirectory(
		SIRENGINE_TEMP_VSTRING( "%s/Assets", g_ContentBrowser.m_Windows.back()->GetBaseDir().Path.c_str() )
	);
	
	g_ContentBrowser.m_Windows.back()->AddSubDir( g_ContentBrowser.m_Windows.back()->GetBaseDir(), "Scripts" );
	FileView_t *ScriptDir = g_ContentBrowser.m_Windows.back()->GetDirectory(
		SIRENGINE_TEMP_VSTRING( "%s/Scripts", g_ContentBrowser.m_Windows.back()->GetBaseDir().Path.c_str() )
	);

	RenderLib::TextureInit_t textureInfo;

	//NOTE: BROKEN -> SEGFAULTS ON CBrowserInstance::Draw()

	g_ContentBrowser.m_pDirectoryIcon = new CMaterial();
	g_ContentBrowser.m_pFileIcon = new CMaterial();
	g_ContentBrowser.m_pAudioIcon = new CMaterial();
	g_ContentBrowser.m_pIniFileIcon = new CMaterial();
	g_ContentBrowser.m_pCsvFileIcon = new CMaterial();
	g_ContentBrowser.m_pJsonFileIcon = new CMaterial();
	g_ContentBrowser.m_pXmlFileIcon = new CMaterial();
	g_ContentBrowser.m_pMaterialIcon = new CMaterial();

	ResourceLoader_t directoryIcon{ "Valden/Bitmaps/DirectoryIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pDirectoryIcon ), RES_MATERIAL };
	ResourceLoader_t fileIcon{ "Valden/Bitmaps/FileIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pFileIcon ), RES_MATERIAL };
	ResourceLoader_t materialIcon{ "Valden/Bitmaps/MaterialIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pMaterialIcon ), RES_MATERIAL };
	ResourceLoader_t audioIcon{ "Valden/Bitmaps/AudioIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pAudioIcon ), RES_MATERIAL };
	ResourceLoader_t jsonIcon{ "Valden/Bitmaps/JsonIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pJsonFileIcon ), RES_MATERIAL };
	ResourceLoader_t xmlIcon{ "Valden/Bitmaps/XmlIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pXmlFileIcon ), RES_MATERIAL };
	ResourceLoader_t csvIcon{ "Valden/Bitmaps/CsvIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pCsvFileIcon ), RES_MATERIAL };
	ResourceLoader_t iniIcon{ "Valden/Bitmaps/IniIcon.png", Cast<CResourceDef>( g_ContentBrowser.m_pIniFileIcon ), RES_MATERIAL };

	CResourceManager::Get().AddResource( directoryIcon );
	CResourceManager::Get().AddResource( fileIcon );
	CResourceManager::Get().AddResource( materialIcon );
	CResourceManager::Get().AddResource( audioIcon );
	CResourceManager::Get().AddResource( jsonIcon );
	CResourceManager::Get().AddResource( xmlIcon );
	CResourceManager::Get().AddResource( csvIcon );
	CResourceManager::Get().AddResource( iniIcon );
}

void CContentBrowser::Draw( void )
{
	for ( auto& it : m_Windows ) {
		it->Draw();
	}
}

void CContentBrowser::Dock( void )
{
	CEditorApplication::Get().DockWindowBottom( "Content Browser" );
}

};
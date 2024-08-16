#include "ContentBrowser.h"
#include "../Project/ProjectManager.h"
#include "FileView.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <Engine/RenderLib/Backend/OpenGL/GLCommon.h>

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
	
	g_ContentBrowser.m_Windows.back()->AddSubDir( *AssetDir, "Audio" );
	g_ContentBrowser.m_Windows.back()->AddSubDir( *AssetDir, "Actors" );
	g_ContentBrowser.m_Windows.back()->AddSubDir( *AssetDir, "Maps" );
	g_ContentBrowser.m_Windows.back()->AddSubDir( *AssetDir, "Textures" );

	g_ContentBrowser.m_Windows.back()->AddSubDir( g_ContentBrowser.m_Windows.back()->GetBaseDir(), "Scripts" );
	FileView_t *ScriptDir = g_ContentBrowser.m_Windows.back()->GetDirectory(
		SIRENGINE_TEMP_VSTRING( "%s/Scripts", g_ContentBrowser.m_Windows.back()->GetBaseDir().Path.c_str() )
	);

	g_ContentBrowser.m_Windows.back()->AddSubDir( *ScriptDir, "Classes" );

	RenderLib::TextureInit_t textureInfo;

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/DirectoryIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pDirectoryIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/FileIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pFileIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/AudioIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pAudioIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/MaterialIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pMaterialIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	ThumbnailSize.Register();
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
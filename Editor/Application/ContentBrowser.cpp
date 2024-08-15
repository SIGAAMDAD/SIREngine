#include "ContentBrowser.h"
#include "Project/ProjectManager.h"
#include <Engine/RenderLib/Backend/RenderContext.h>
#include <boost/filesystem.hpp>

namespace Valden {

static CVar<float> ThumbnailSize(
	"ui.ThumbnailSize",
	132.0f,
	Cvar_Save,
	"Sets the ContentBrowser Thumbnail Size",
	CVG_USERINFO
);
SIRENGINE_DEFINE_LOG_CATEGORY( ContentBrowser, ELogLevel::Info );
CContentBrowser CContentBrowser::g_ContentBrowser;

void CContentBrowser::LoadAssetTree( const FileSystem::CFilePath& directory )
{
	SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Info, "Adding AssetTree \"%s\" to ContentBrowser Cache...", directory.c_str() );

	m_AssetTree.AddSubDirectory( m_AssetTree.GetBase(), "Assets" );

	m_AssetTree.AddSubDirectory( *m_AssetTree.GetDirectory( "Assets" ), "Actors" );
	m_AssetTree.AddSubDirectory( *m_AssetTree.GetDirectory( "Assets" ), "Audio" );
	m_AssetTree.AddSubDirectory( *m_AssetTree.GetDirectory( "Assets" ), "Maps" );
	m_AssetTree.AddSubDirectory( *m_AssetTree.GetDirectory( "Assets" ), "Textures" );
}

void CContentBrowser::LoadScriptTree( const FileSystem::CFilePath& directory )
{
	SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Info, "Adding ScriptTree \"%s\" to ContentBrowser Cache...", directory.c_str() );

	m_AssetTree.AddSubDirectory( m_AssetTree.GetBase(), "Scripts" );

	m_AssetTree.AddSubDirectory( *m_AssetTree.GetDirectory( "Scripts" ), "Classes" );
}

void CContentBrowser::Init( void )
{
	CEditorApplication::Get().AddWidget( &g_ContentBrowser );

	g_ContentBrowser.LoadAssetTree( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Assets", CProjectManager::Get()->GetProject()->GetName().c_str() ) );
	g_ContentBrowser.LoadScriptTree( SIRENGINE_TEMP_VSTRING( "Valden/Projects/%s/Scripts", CProjectManager::Get()->GetProject()->GetName().c_str() ) );
	g_ContentBrowser.m_Favorites.reserve( 256 );

	RenderLib::TextureInit_t textureInfo;

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/DirectoryIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pDirectoryIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.filePath = "Valden/Bitmaps/FileIcon.png";
	textureInfo.nFormat = RenderLib::TF_RGBA;
	g_ContentBrowser.m_pDirectoryIcon = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );

	ThumbnailSize.Register();
}

void CContentBrowser::Draw( void )
{
	const float cellSize = ThumbnailSize.GetValue();
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = 0, columnIndex;

	ImGui::Begin( "Content Browser", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

	if ( ImGui::BeginTable( "##ContentBrowserTable", 2, ImGuiTableFlags_Resizable ) ) {
		ImGui::TableNextColumn();
		if ( ImGui::BeginTable( "##ContentBrowserSidePanel", 1, ImGuiTableFlags_ScrollY ) ) {
			ImGui::TableNextColumn();
			if ( ImGui::CollapsingHeader( "Favorites##ContentBrowserSidePanelFavorites" ) ) {
				for ( auto& it : m_Favorites ) {
					const char *pName = strrchr( it.pFavorite->c_str(), '/' ) + 1;
					if ( ImGui::MenuItem( pName ) ) {
					}
				}
			}
			if ( ImGui::CollapsingHeader( "Project##ContentBrowserSidePanelProject" ) ) {
				m_AssetTree.Draw();
			}
			ImGui::EndTable();
		}

		ImGui::TableNextColumn();

		columnCount = (int)( panelWidth / cellSize );
		if ( columnCount < 1 ) {
			columnCount = 1;
		}

		if ( ImGui::BeginTable( "##ContentBrowserMainPanelTable", columnCount, ImGuiTableFlags_ScrollY ) ) {
//			ImGui::TableNextColumn();

//			ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.0f, 0.0f ) );

			columnIndex = 0;

			FileView_t *pDirectory = m_AssetTree.GetSelectedDir();

//			ImGui::Columns( columnCount, NULL, false );

			if ( pDirectory ) {
				for ( auto& file : pDirectory->FileList ) {
					ImGui::PushID( file.c_str() );
					if ( ImGui::ImageButtonEx( ImGui::GetID( m_pFileIcon ), IMGUI_TEXTURE_ID( m_pFileIcon ),
						{ ThumbnailSize.GetValue(), ThumbnailSize.GetValue() }, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 } ) )
					{
					}
					ImGui::PopID();

//					ImGui::NextColumn();
					ImGui::TableNextColumn();
				}
				for ( auto& directory : pDirectory->Directories ) {
					ImGui::PushID( directory.Path.c_str() );
					if ( ImGui::ImageButtonEx( ImGui::GetID( m_pDirectoryIcon ), IMGUI_TEXTURE_ID( m_pDirectoryIcon ),
						{ ThumbnailSize.GetValue(), ThumbnailSize.GetValue() }, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, { 1, 1, 1, 1 } ) )
					{
						m_AssetTree.SetSelected( &directory );
					}
					if ( ImGui::BeginDragDropSource() ) {
						ImGui::EndDragDropSource();
					}
					ImGui::PopID();

//					ImGui::NextColumn();
					ImGui::TableNextColumn();
				}
			}

//			ImGui::Columns( 1 );
			ImGui::EndTable();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void CContentBrowser::Dock( void )
{
	CEditorApplication::Get().DockWindowBottom( "Content Browser" );
}

};
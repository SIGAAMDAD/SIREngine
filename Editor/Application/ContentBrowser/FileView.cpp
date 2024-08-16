#include "FileView.h"
#include "BrowserInstance.h"

namespace Valden::ContentBrowser {

void CFileView::DrawPopup( void )
{
	if ( ImGui::BeginPopup( "Folder Utilities##FileTreeDirectoryUtilities" ) ) {
		ImGui::SetWindowPos( m_MousePopupPosition );

		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_FOLDER_PLUS "New Folder##NewFolder_FileTreeDirectoryUtilities" ) ) {
			m_pPopupDir->Directories.emplace_back();
			m_pPopupDir->Directories.back().DirectoryName = "New Folder";
			m_pPopupDir->Directories.back().Path = SIRENGINE_TEMP_VSTRING( "%s/New Folder", m_pPopupDir->Path.c_str() );
		}
		ITEM_TOOLTIP_STRING( "Creates a new folder in selected folder" );
		if ( ImGui::MenuItem( ICON_FA_FOLDER_OPEN
			"Show Folder in New Content Browser##ShowFolder_FileTreeDirectoryUtilities" ) )
		{

		}
		ITEM_TOOLTIP_STRING( "Opens a new Content Browser window set at the selected folder" );
		ImGui::Unindent( 0.5f );

		ImGui::SeparatorText( "Folder Actions" );
		ImGui::Indent( 0.5f );
		if ( ImGui::MenuItem( ICON_FA_STAR "Add To Favorites##AddToFav_FileTreeDirectoryUtilities" ) ) {
			m_pInstance->m_Favorites.push_back( { m_pPopupFile,
				m_pInstance->m_AssetTree.GetDirectory( *m_pPopupFile ) != NULL } );
			ImGui::CloseCurrentPopup();
		}
		ITEM_TOOLTIP_STRING( "Flags selected folder as favorite" );
		if ( ImGui::MenuItem( ICON_FA_EDIT "Rename##Rename_FileTreeDirectoryUtilities" ) ) {
		}
		ITEM_TOOLTIP_STRING( "Rename selected folder" );
		ImGui::Unindent( 0.5f );

		ImGui::EndPopup();
	}
}

void CFileView::DrawDirectoryTree( FileView_t& fileView )
{
	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_None;
	if ( !fileView.Directories.empty() ) {
		treeNodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow;
	} else {
		treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}
	if ( m_pSelectedDir == &fileView ) {
		treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	if ( ImGui::TreeNodeEx( (void *)fileView.DirectoryName.c_str(), treeNodeFlags,
		ICON_FA_FOLDER "%s", fileView.DirectoryName.c_str() ) )
	{
		if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
			m_pSelectedDir = &fileView;
		}
		if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
			ImGui::OpenPopup( "Folder Utilities##FileTreeDirectoryUtilities" );
			m_MousePopupPosition = ImGui::GetMousePos();
			m_pPopupFile = &fileView.Path;
			m_pPopupDir = &fileView;
		}
		if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
			ImGui::SetTooltip( fileView.Path.c_str() );
		}
		for ( auto& dir : fileView.Directories ) {
			DrawDirectoryTree( dir );
		}
		ImGui::TreePop();
	}
	if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
		ImGui::OpenPopup( "Folder Utilities##FileTreeDirectoryUtilities" );
		m_MousePopupPosition = ImGui::GetMousePos();
		m_pPopupFile = &fileView.Path;
		m_pPopupDir = &fileView;
	}
	if ( m_pPopupDir == &fileView ) {
		DrawPopup();
	}
}

};
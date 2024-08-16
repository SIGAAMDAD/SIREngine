#ifndef __FILE_TREE_VIEW_H__
#define __FILE_TREE_VIEW_H__

#pragma once

#include <Engine/Util/CString.h>
#include <Engine/Util/CVector.h>

typedef struct FileView {
	CString DirectoryName;
	CString Path;
	FileView *Parent;
	CVector<FileView> Directories;
	CVector<CString> FileList;

	inline bool operator==( const FileView& other ) const
	{ return DirectoryName == other.DirectoryName && Path == other.Path; }
	inline bool HasChild( const FileView& fileView ) const
	{ return eastl::find( Directories.cbegin(), Directories.cend(), fileView ) != Directories.cend(); }
	inline bool HasParent( const FileView *fileView ) const
	{
		for ( FileView *it = Parent; it != NULL; it = it->Parent ) {
			if ( it == fileView ) {
				return true;
			}
		}
		return false;
	}
} FileView_t;

template<bool bIncludeFiles, bool bShowFullTooltipPath, bool bDrawFiles>
class CFileTreeView
{
public:
	CFileTreeView( void )
		: m_pSelected( NULL )
	{ }
	~CFileTreeView()
	{ }

	inline void Draw( void (*UtilityPopup)( const CString&, bool, FileView_t * ) )
	{
		for ( auto& it : m_FileList.Directories ) {
			DrawDirectory( it, UtilityPopup );
		}
	}

	inline bool HasSelectedDir( void ) const
	{ return m_pSelectedDir != NULL; }
	inline FileView_t *GetSelectedDir( void ) const
	{ return m_pSelectedDir; }
	inline void SetSelectedDir( FileView_t *pDirectory )
	{ m_pSelectedDir = pDirectory; }

	inline bool HasSelected( void ) const
	{ return m_pSelected != NULL; }
	inline const CString& GetSelected( void ) const
	{ return *m_pSelected; }
	inline void SetSelected( FileView_t *pDirectory )
	{ m_pSelected = &pDirectory->Path; }
	inline FileView_t& GetBase( void )
	{ return m_FileList; }

	inline FileView_t& AddDirectory( const CString& name, FileView_t *pOwner = NULL )
	{
		FileView_t *pView = NULL;
		if ( pOwner ) {
			pView = &pOwner->Directories.emplace_back();
		} else {
			pView = &m_FileList.Directories.emplace_back();
			pOwner = &m_FileList;
		}
		pView->Path = name;
		pView->Parent = pOwner;
		pView->DirectoryName = strrchr( name.c_str(), '/' ) + 1;

		if SIRENGINE_CONSTEXPR ( bIncludeFiles ) {
			// don't just fetch folder names
			g_pFileSystem->AddCacheDirectory( name.c_str() );
			SIREngine::FileSystem::CFileList *fileList = g_pFileSystem->ListFiles( name.c_str(), "" );

			if ( !fileList ) {
				SIRENGINE_WARNING( "Got 0 files for directory \"%s\".", name.c_str() );
				return *pView;
			}
			pView->FileList.reserve( fileList->GetList().size() );

			for ( const auto& it : fileList->GetList() ) {
				pView->FileList.emplace_back( it.c_str() );
			}

			delete fileList;
		}

		return *pView;
	}

	inline FileView_t *GetDirectory( const CString& name )
	{
		for ( auto& it : m_FileList.Directories ) {
			if ( it.Path == name ) {
				return &it;
			}
		}
		return NULL;
	}

	inline FileView_t& AddSubDirectory( FileView_t& fileView, const CString& name )
	{ return AddDirectory( name, &fileView ); }
private:
	void DrawDirectory( FileView_t& fileView, void (*UtilityPopup)( const CString&, bool, FileView_t * ) )
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
			if ( UtilityPopup ) {
				if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
					ImGui::OpenPopup( "File Utilities##FileTreeDirectoryUtilities" );
					m_MousePopupPosition = ImGui::GetMousePos();
					m_pPopupFile = &fileView.Path;
					m_bPopupIsDirectory = true;
				}
				if ( ImGui::BeginPopup( "File Utilities##FileTreeDirectoryUtilities" ) ) {
					ImGui::SetWindowPos( m_MousePopupPosition );
					ImGui::SetWindowFocus();
					UtilityPopup( *m_pPopupFile, m_bPopupIsDirectory, &fileView );
					ImGui::EndPopup();
				}
			}
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
				ImGui::SetTooltip( fileView.Path.c_str() );
			}
			for ( auto& dir : fileView.Directories ) {
				DrawDirectory( dir, UtilityPopup );
			}
			if SIRENGINE_CONSTEXPR ( bDrawFiles ) {
				for ( auto& file : fileView.FileList ) {
					const char *pFileName = strrchr( file.c_str(), '/' ) + 1;
					if ( ImGui::Selectable( pFileName, ( m_pSelected == &file ) ) ) {
						m_pSelected = &file;
					}
					if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
						if SIRENGINE_CONSTEXPR ( bShowFullTooltipPath ) {
							ImGui::SetTooltip( file.c_str() );
						} else {
							ImGui::SetTooltip( pFileName );
						}
					}
					if ( UtilityPopup ) {
						if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
							ImGui::OpenPopup( "File Utilities##FileTreeFileUtilities" );
							m_MousePopupPosition = ImGui::GetMousePos();
							m_pPopupFile = &file;
							m_bPopupIsDirectory = false;
						}
						if ( ImGui::BeginPopup( "File Utilities##FileTreeFileUtilities" ) ) {
							ImGui::SetWindowPos( m_MousePopupPosition );
							ImGui::SetWindowFocus();
							UtilityPopup( *m_pPopupFile, m_bPopupIsDirectory, NULL );
							ImGui::EndPopup();
						}
					}
				}
			}
			ImGui::TreePop();
		}
		else {
			if ( UtilityPopup ) {
				if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
					ImGui::OpenPopup( "File Utilities##FileTreeDirectoryUtilities" );
					m_MousePopupPosition = ImGui::GetMousePos();
					m_pPopupFile = &fileView.Path;
					m_bPopupIsDirectory = true;
				}
				if ( ImGui::BeginPopup( "File Utilities##FileTreeDirectoryUtilities" ) ) {
					ImGui::SetWindowPos( m_MousePopupPosition );
					ImGui::SetWindowFocus();
					UtilityPopup( *m_pPopupFile, m_bPopupIsDirectory, &fileView );
					ImGui::EndPopup();
				}
			}
		}
	}

	FileView_t m_FileList;
	FileView_t *m_pSelectedDir;
	CString *m_pSelected;

	CString *m_pPopupFile;
	ImVec2 m_MousePopupPosition;
	bool m_bPopupIsDirectory;
};

#endif
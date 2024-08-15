#ifndef __FILE_TREE_VIEW_H__
#define __FILE_TREE_VIEW_H__

#pragma once

#include <Engine/Util/CString.h>
#include <Engine/Util/CVector.h>

typedef struct FileView {
	CString DirectoryName;
	CString Path;
	CVector<FileView> Directories;
	CVector<CString> FileList;
} FileView_t;

template<bool bIncludeFiles, bool bShowFullTooltipPath>
class CFileTreeView
{
public:
	CFileTreeView( void )
		: m_pSelected( NULL )
	{ }
	~CFileTreeView()
	{ }

	inline void Draw( void )
	{
		for ( auto& it : m_FileList.Directories ) {
			DrawDirectory( it );
		}
	}

	inline bool HasSelectedDir( void ) const
	{ return m_pSelectedDir != NULL; }
	inline FileView_t *GetSelectedDir( void ) const
	{ return m_pSelectedDir; }

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
		}
		pView->DirectoryName = name;

		if SIRENGINE_CONSTEXPR ( bIncludeFiles ) {
			// don't just fetch folder names
			SIREngine::FileSystem::CFileList *fileList = g_pFileSystem->ListFiles( name.c_str(), "" );
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

	inline void AddSubDirectory( FileView_t& fileView, const CString& name )
	{
		fileView.Directories.emplace_back();
		fileView.Directories.back().Path = name;
		fileView.Directories.back().DirectoryName = name;
		SIRENGINE_LOG( "Added SubDirectory \"%s\" to FileView \"%s\".", fileView.Directories.back().Path.c_str(),
			fileView.DirectoryName.c_str() );
	}
private:
	void DrawDirectory( FileView_t& fileView )
	{
		if ( ImGui::TreeNodeEx( (void *)fileView.DirectoryName.c_str(),
			ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_OpenOnArrow |
			( m_pSelectedDir == &fileView ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None ),
			ICON_FA_FOLDER "%s", fileView.DirectoryName.c_str() ) )
		{
			if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) {
				m_pSelectedDir = &fileView;
				SIRENGINE_LOG( "Activating FileTree Item \"%s\".", fileView.DirectoryName.c_str() );
			}
			if ( ImGui::IsItemHovered( ImGuiHoveredFlags_DelayNone | ImGuiHoveredFlags_AllowWhenDisabled ) ) {
				ImGui::SetTooltip( fileView.Path.c_str() );
			}
			for ( auto& dir : fileView.Directories ) {
				DrawDirectory( dir );
			}
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
			}

			ImGui::TreePop();
		}
	}

	FileView_t m_FileList;
	FileView_t *m_pSelectedDir;
	CString *m_pSelected;
};

#endif
#ifndef __VALDEN_CONTENTBROWSER_FILEVIEW_H__
#define __VALDEN_CONTENTBROWSER_FILEVIEW_H__

#pragma once

#include "../Application.h"

namespace Valden::ContentBrowser {
	SIRENGINE_DECLARE_LOG_CATEGORY( ContentBrowser, ELogLevel::Info );

	class CBrowserInstance;

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

	class CFileView
	{
	public:
		CFileView( CBrowserInstance *pInstance )
			: m_pInstance( pInstance ), m_pPopupDir( NULL ), m_pSelectedDir( NULL ), m_pSelected( NULL ),
			m_pPopupFile( NULL ), m_MousePopupPosition( { 0, 0 } ), m_bPopupIsDirectory( false )
		{ }
		~CFileView()
		{ }

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
			if ( ( pView = GetDirectory( name ) ) != NULL ) {
				return *pView; // already added
			}
			if ( pOwner ) {
				pView = &pOwner->Directories.emplace_back();
			} else {
				pView = &m_FileList.Directories.emplace_back();
				pOwner = &m_FileList;
			}
			pView->Path = name;
			pView->Parent = pOwner;
			pView->DirectoryName = strrchr( name.c_str(), '/' ) + 1;

			SIRENGINE_LOG_LEVEL( ContentBrowser, ELogLevel::Info, "Added Directory \"%s\" to FileView Cache.", name.c_str() );

			g_pFileSystem->AddCacheDirectory( name.c_str() );
			
			SIREngine::FileSystem::CFileList *fileList = g_pFileSystem->ListFiles( name.c_str(), NULL );

			if ( !fileList ) {
				SIRENGINE_WARNING( "Got 0 files for directory \"%s\".", name.c_str() );
			}
			else {
				pView->FileList.reserve( fileList->GetList().size() );
				for ( const auto& it : fileList->GetList() ) {
					pView->FileList.emplace_back( it.c_str() );
				}
				delete fileList;
			}

			SIREngine::FileSystem::CFileList *dirList = g_pFileSystem->ListFiles( name.c_str(), NULL, true );

			if ( !dirList ) {
				SIRENGINE_WARNING( "Got 0 subdirectories for directory \"%s\".", name.c_str() );
			}
			else {
				pView->Directories.reserve( dirList->GetSubDirs().size() );
				for ( const auto& it : dirList->GetSubDirs() ) {
					AddDirectory( it.c_str(), pView );
				}
				delete dirList;
			}

			return *pView;
		}

		inline FileView_t *GetDirectory( const CString& name )
		{ return SearchForDir( m_FileList, name ); }

		inline FileView_t& AddSubDirectory( FileView_t& fileView, const CString& name )
		{ return AddDirectory( name, &fileView ); }

		inline void Draw( void )
		{
			for ( auto& it : m_FileList.Directories ) {
				DrawDirectoryTree( it );
			}
		}
	private:
		inline FileView_t *SearchForDir( FileView_t& Parent, const CString& dirName )
		{
			for ( auto& it : Parent.Directories ) {
				if ( it.Path == dirName || it.DirectoryName.find( dirName ) != CString::npos ) {
					return &it;
				}
				SearchForDir( it, dirName );
			}
			return NULL;
		}

		void DrawDirectoryTree( FileView_t& fileView );
		void DrawPopup( void );

		CBrowserInstance *m_pInstance;
		FileView_t m_FileList;
		FileView_t *m_pPopupDir;
		FileView_t *m_pSelectedDir;
		CString *m_pSelected;

		CString *m_pPopupFile;
		ImVec2 m_MousePopupPosition;
		bool m_bPopupIsDirectory;
	};
};

#endif
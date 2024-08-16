#ifndef __VALDEN_CONTENTBROWSER_BROWSERINSTANCE_H__
#define __VALDEN_CONTENTBROWSER_BROWSERINSTANCE_H__

#pragma once

#include "../Application.h"
#include "FileView.h"
#include <EASTL/shared_ptr.h>

namespace Valden::ContentBrowser {
	typedef struct ViewFavorite {
		ViewFavorite( CString *_pFavorite, bool _bIsDirectory )
			: pFavorite( _pFavorite ), bIsDirectory( _bIsDirectory )
		{ }

		CString *pFavorite;
		bool bIsDirectory;
	} ViewFavorite_t;
	
	enum class EContentType {
		File,
		Material,
		Audio,
		Animation,
	};

	typedef struct FileInfo {
		const CString *fileName;
		EContentType nType;
		uint64_t nDiskSize;
	} FileInfo_t;

	class CBrowserInstance {
		friend class CContentBrowser;
		friend class CFileView;
	public:
		CBrowserInstance( const FileSystem::CFilePath& directory  );
		~CBrowserInstance()
		{ }

		void Init( const FileSystem::CFilePath& directory );
		void AddSubDir( FileView_t& Parent, const CString& dirName );
		void Draw( void );

		FileView_t *GetDirectory( const CString& dirPath )
		{ return m_AssetTree.GetDirectory( dirPath ); }
		FileView_t& GetBaseDir( void )
		{ return m_AssetTree.GetBase(); }
	private:
		void LoadDirectoryTree( const FileSystem::CFilePath& directory );
		void AddCacheDir( const FileView_t& fileView );;

		void DrawDirectoryTree( FileView_t& fileView );

		CVector<ViewFavorite_t> m_Favorites;
		CFileView m_AssetTree;

		eastl::unordered_map<CString, FileInfo_t> m_FileDatas;

		ImVec2 m_MousePopupPosition;
		CString *m_pPopupFile;
	};
};

#endif
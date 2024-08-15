#ifndef __VALDEN_CONTENT_BROWSER_H__
#define __VALDEN_CONTENT_BROWSER_H__

#pragma once

#include "Application.h"
#include "Util/FileTreeView.h"
#include <Engine/RenderLib/Backend/RenderTexture.h>
#include <boost/filesystem.hpp>

namespace Valden {
	typedef struct {
		CString *pFavorite;
		bool bIsDirectory;
	} ViewFavorite_t;

	class CContentBrowser : public IEditorWidget
	{
	public:
		CContentBrowser( void )
			: IEditorWidget( "ContentBrowser" )
		{ }
		virtual ~CContentBrowser() override
		{ }

		static void Init( void );

		virtual void Draw( void ) override;
		virtual void Dock( void ) override;

		SIRENGINE_FORCEINLINE static CContentBrowser& Get( void )
		{ return g_ContentBrowser; }
	private:
		void DrawDirectory( const boost::filesystem::path& directory );
		void LoadAssetTree( const FileSystem::CFilePath& directory );
		void LoadScriptTree( const FileSystem::CFilePath& directory );

		CVector<ViewFavorite_t> m_Favorites;
		CFileTreeView<false, true> m_AssetTree;

		RenderLib::Backend::IRenderTexture *m_pDirectoryIcon;
		RenderLib::Backend::IRenderTexture *m_pFileIcon;

		static CContentBrowser g_ContentBrowser;
	};
};

#endif
#ifndef __VALDEN_CONTENT_BROWSER_H__
#define __VALDEN_CONTENT_BROWSER_H__

#pragma once

#include "../Application.h"
#include <Engine/RenderLib/Backend/RenderTexture.h>
#include "BrowserInstance.h"
#include <Engine/Core/ResourceDef.h>

/*
* TODO: rewrite the CBrowserInstance class so that we're only loading the
* directory tree once and not reloading and reallocating (possibly) hundreds
* of copies of the same filepaths
*/

namespace Valden::ContentBrowser {
	class CContentBrowser : public IEditorWidget
	{
		friend class CBrowserInstance;
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
		CVector<eastl::unique_ptr<CBrowserInstance>> m_Windows;

		CResourceDef *m_pDirectoryIcon;
		CResourceDef *m_pFileIcon;
		CResourceDef *m_pAudioIcon;
		CResourceDef *m_pMaterialIcon;

		CResourceDef *m_pJsonFileIcon;
		CResourceDef *m_pIniFileIcon;
		CResourceDef *m_pCsvFileIcon;
		CResourceDef *m_pXmlFileIcon;

		static CContentBrowser g_ContentBrowser;
	};

	SIRENGINE_DECLARE_LOG_CATEGORY( ContentBrowser, ELogLevel::Info );
};

#endif
#ifndef __VALDEN_CONTENT_BROWSER_H__
#define __VALDEN_CONTENT_BROWSER_H__

#pragma once

#include "../Application.h"
#include <Engine/RenderLib/Backend/RenderTexture.h>
#include "BrowserInstance.h"
#include <EASTL/shared_ptr.h>

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

		RenderLib::Backend::IRenderTexture *m_pDirectoryIcon;
		RenderLib::Backend::IRenderTexture *m_pFileIcon;
		RenderLib::Backend::IRenderTexture *m_pAudioIcon;
		RenderLib::Backend::IRenderTexture *m_pMaterialIcon;

		static CContentBrowser g_ContentBrowser;
	};

	SIRENGINE_DECLARE_LOG_CATEGORY( ContentBrowser, ELogLevel::Info );
};

#endif
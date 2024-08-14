#ifndef __GUILIB_TEXTMANAGER_H__
#define __GUILIB_TEXTMANAGER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "GUILib.h"
#include <EASTL/unordered_map.h>
#include <EASTL/shared_ptr.h>
#include <Engine/Core/Compiler.h>
#include <Engine/Util/CString.h>
#include <Engine/Core/MathLib/Types.h>
#include "Font.h"

namespace GUILib {
	/*
	typedef struct {
		CIVec2 size;
		CIVec2 bearing;
		uint32_t nAdvance;
	} Glyph_t;

	typedef struct {
		eastl::shared_ptr<CResourceDef> pTexture;
		eastl::unordered_map<char, Glyph_t> m_GlyphCache;
	} FontAtlas_t;
	*/

	class CTextManager
	{
	public:
		CTextManager( void );
		~CTextManager();

		eastl::shared_ptr<CFont> LoadFont( const SIREngine::FileSystem::CFilePath& fontFile );

		SIRENGINE_FORCEINLINE static eastl::unique_ptr<CTextManager>& Get( void )
		{ return g_pFontManager; }
	private:
		static void LoadThread( void );

		eastl::unordered_map<SIREngine::FileSystem::CFilePath, eastl::shared_ptr<CFont>> m_FontCache;
		
		static eastl::unique_ptr<CTextManager> g_pFontManager;
	};
};

#endif
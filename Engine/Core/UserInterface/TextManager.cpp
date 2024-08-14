#include <Engine/Core/SIREngine.h>
#include "TextManager.h"
#include <Engine/Core/ThreadSystem/Thread.h>
#include <Engine/RenderLib/ShaderCache.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <EASTL/unordered_map.h>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>

namespace GUILib {

struct CFontLoader
{
	CFontLoader( void )
	{ }
	CFontLoader( const SIREngine::FileSystem::CFilePath& fontPath, eastl::shared_ptr<CFont> *pFont )
		: path( fontPath ), pResource( pFont )
	{ }

	SIREngine::FileSystem::CFilePath path;
	eastl::shared_ptr<CFont> *pResource;
};

static SIREngine::CThread s_FontLoaderThread( "FontLoaderThread" );
static SIREngine::CThreadAtomic<bool> s_bDoneLoading;
static SIREngine::CThreadMutex s_hLoadLock;
static boost::lockfree::queue<CFontLoader, boost::lockfree::capacity<256>> s_LoadQueue;

eastl::unique_ptr<CTextManager> CTextManager::g_pFontManager;

CTextManager::CTextManager( void )
{
	s_FontLoaderThread.Start( CTextManager::LoadThread );
}

CTextManager::~CTextManager()
{
}

eastl::shared_ptr<CFont> CTextManager::LoadFont( const SIREngine::FileSystem::CFilePath& fontFile )
{
	eastl::shared_ptr<CFont> font;

	s_LoadQueue.push( CFontLoader( fontFile, &font ) );

	return font;
}

void CTextManager::LoadThread( void )
{
	CFontLoader fontData;

	while ( !s_bDoneLoading.load() ) {
		if ( s_LoadQueue.pop( fontData ) ) {
			SIREngine::CThreadAutoLock<SIREngine::CThreadMutex> _( s_hLoadLock );
			*fontData.pResource = eastl::make_shared<CFont>( fontData.path );
			g_pFontManager->m_FontCache.try_emplace( fontData.path, *fontData.pResource );
		} else {
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 100 ) );
		}
	}
}

};
#include <Engine/Core/SIREngine.h>
#include "TextManager.h"
#include <Engine/Core/ThreadSystem/Thread.h>
#include <Engine/RenderLib/ShaderCache.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <EASTL/unordered_map.h>
#include <boost/lockfree/queue.hpp>
#include <boost/thread.hpp>
#include "../ResourceManager.h"

namespace SIREngine::GUILib {

struct CFontLoader
{
	CFontLoader( void )
	{ }
	CFontLoader( const FileSystem::CFilePath& fontPath, CFont **pFont )
		: path( fontPath ), pResource( pFont )
	{ }

	FileSystem::CFilePath path;
	CFont **pResource;
};

static CThread s_FontLoaderThread( "FontLoaderThread" );
static CThreadAtomic<bool> s_bDoneLoading;
static CThreadMutex s_hLoadLock;
static boost::lockfree::queue<CFontLoader, boost::lockfree::capacity<256>> s_LoadQueue;

eastl::unique_ptr<CTextManager> CTextManager::g_pFontManager;

CTextManager::CTextManager( void )
{
	s_FontLoaderThread.Start( CTextManager::LoadThread );
}

CTextManager::~CTextManager()
{
}

CFont *CTextManager::LoadFont( const FileSystem::CFilePath& fontFile )
{
	CFont *pFont;

	s_LoadQueue.push( CFontLoader( fontFile, &pFont ) );

	return pFont;
}

void CTextManager::LoadThread( void )
{
	CFontLoader fontData;

	while ( !s_bDoneLoading.load() ) {
		if ( s_LoadQueue.pop( fontData ) ) {
			CThreadAutoLock<CThreadMutex> _( s_hLoadLock );
			SIRENGINE_LOG_LEVEL( SyncedResourceLoader, ELogLevel::Info, "Synchronously loading font \"%s\"...", fontData.path.c_str() );
			*fontData.pResource = new CFont( fontData.path );
			g_pFontManager->m_FontCache.try_emplace( fontData.path, *fontData.pResource );
		} else {
			boost::this_thread::sleep_for( boost::chrono::milliseconds( 100 ) );
		}
	}
}

};
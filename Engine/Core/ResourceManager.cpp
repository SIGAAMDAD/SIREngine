#include "ResourceManager.h"
#include <Engine/Core/FileSystem/FileSystem.h>
#include <boost/lockfree/queue.hpp>

namespace SIREngine {

SIRENGINE_DEFINE_LOG_CATEGORY( SyncedResourceLoader, ELogLevel::Info );

CResourceManager CResourceManager::g_ResourceManager;

static CThread s_LoaderThread( "ResourceLoadThread" );
static CThreadMutex s_LoadMutex;
static CThreadAtomic<bool> s_bDoneLoading;
static CVector<ResourceLoader_t> s_ResourceQueue;

CResourceManager::CResourceManager( void )
{
}

CResourceManager::~CResourceManager()
{
}

void CResourceManager::AddResource( ResourceLoader_t& resourceData )
{
	SIRENGINE_LOG_LEVEL( SyncedResourceLoader, ELogLevel::Info, "- Loading CResourceDef \"%s\"...", resourceData.FilePath.c_str() );
	CThreadAutoLock<CThreadMutex> _( s_LoadMutex );
	s_ResourceQueue.emplace_back( resourceData );
}

void CResourceManager::LoadThread( void )
{
	while ( 1 ) {
		CThreadAutoLock<CThreadMutex> _( s_LoadMutex );
		while ( s_ResourceQueue.size() ) {
			ResourceLoader_t& resourceData = s_ResourceQueue.back();
//			if ( resourceData.pResourcePointer ) {
//				*resourceData.pResourcePointer = MakeUnique<CResourceDef>( resourceData.FilePath );
//			} else if ( resourceData.ResourceData ) {
				switch ( resourceData.nType ) {
				case RES_MATERIAL:
					Cast<CMaterial>( resourceData.ResourceData )->Reload( resourceData.FilePath );
					break;
				};
//			}
			s_ResourceQueue.pop_back();
		}
		if ( s_bDoneLoading.load() ) {
			break;
		}
	}
}

void CResourceManager::BeginLoad( void )
{
//	m_ResourcePaths.reserve( 4 );
//	RegisterResourceFolder( "Resources/Shaders/Vulkan", ".spv" );
//	RegisterResourceFolder( "Resources/Shaders/OpenGL", ".glsl" );
//	RegisterResourceFolder( "Resources/Shaders/D3D11", ".hlsl" );

	s_ResourceQueue.reserve( 2048 );

	s_bDoneLoading.store( false );
	s_LoaderThread.Start( CResourceManager::LoadThread );
}

void CResourceManager::EndLoad( void )
{
	s_bDoneLoading.store( true );

	// give it a little bit of time to finish up anything
	// its working on
	s_LoaderThread.Join();
}

void CResourceManager::RegisterResourceFolder( const char *pDirectory, const char *pExtension )
{
//	g_pFileSystem->AddCacheDirectory( pDirectory );
//	m_ResourcePaths.emplace_back( pDirectory );
}

};
#include "ResourceManager.h"
#include <Engine/Core/FileSystem/FileSystem.h>

namespace SIREngine {

SIRENGINE_DEFINE_LOG_CATEGORY( SyncedResourceLoader, ELogLevel::Info );

CResourceManager::CResourceManager( void )
{
	m_ResourcePaths.reserve( 4 );
	RegisterResourceFolder( "Resources/Shaders/Vulkan", ".spv" );
	RegisterResourceFolder( "Resources/Shaders/OpenGL", ".glsl" );
	RegisterResourceFolder( "Resources/Shaders/D3D11", ".hlsl" );
}

CResourceManager::~CResourceManager()
{
}

void CResourceManager::RegisterResourceFolder( const char *pDirectory, const char *pExtension )
{
	g_pFileSystem->AddCacheDirectory( pDirectory );
	m_ResourcePaths.emplace_back( pDirectory );
}

};
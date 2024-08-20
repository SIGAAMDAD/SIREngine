#include "ResourceDef.h"
#include <Engine/RenderLib/Backend/RenderTexture.h>
#include <Engine/RenderLib/Backend/RenderContext.h>

using namespace SIREngine;
using namespace SIREngine::RenderLib;

namespace SIREngine {

void CMaterial::Release( void )
{
	if ( m_pTexture ) {
 		delete m_pTexture;
	}
//	RenderLib::Backend::GetRenderContext()->Free( m_pTexture );
}

void CMaterial::Reload( const FileSystem::CFilePath& filePath )
{
	SIREngine_strncpyz( m_szName, filePath.c_str(), sizeof( m_szName ) );

	RenderLib::TextureInit_t textureInfo;
	memset( &textureInfo, 0, sizeof( textureInfo ) );
	textureInfo.bIsGPUOnly = false;
	textureInfo.nChannels = 4;
	textureInfo.filePath = filePath;
	textureInfo.nFormat = TF_RGBA;

	SIRENGINE_LOG( "Loading Material \"%s\"...", filePath.c_str() );

	m_pTexture = RenderLib::Backend::GetRenderContext()->AllocateTexture( textureInfo );
}

};
#include "SIRGLTexture.h"
#include "Renderer/Drivers/OpenGL/SIRGLDevice.h"

SIRGLTexture::SIRGLTexture( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderTextureDesc& desc ) noexcept
    : SIRRenderTexture( device, desc )
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( glDevice.hasExtension( "GL_ARB_direct_state_access" ) ) {
		glDevice.createTextures( GL_TEXTURE_2D, 1, &m_hTextureID );
	} else {
		glDevice.genTextures( 1, &m_hTextureID );
	}
}

SIRGLTexture::~SIRGLTexture() noexcept
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( m_hTextureID != GL_INVALID_VALUE ) {
		glDevice.deleteTextures( 1, &m_hTextureID );
	}
}

#include "SIRGLTexture.h"
#include "Renderer/Drivers/OpenGL/SIRGLDevice.h"

SIRGLTexture::SIRGLTexture( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderTextureDesc& desc ) noexcept
	: SIRRenderTexture( device, desc )
{
}

SIRGLTexture::~SIRGLTexture() noexcept
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( m_hTextureID != GL_INVALID_VALUE ) {
		device.d
	}
}

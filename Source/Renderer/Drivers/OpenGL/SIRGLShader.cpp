#include "SIRGLShader.h"
#include "SIRGLDevice.h"

SIRGLShader::SIRGLShader( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderShaderCode& description ) noexcept
	: SIRRenderShader( device, description )
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();
}

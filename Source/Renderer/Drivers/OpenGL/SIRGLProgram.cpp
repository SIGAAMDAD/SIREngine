#include "SIRGLProgram.h"
#include "Renderer/RHI/SIRRenderProgram.h"
#include <GL/gl.h>

SIRGLProgram::SIRGLProgram( const SIRSharedPtr<SIRRenderDevice>& device, const SIRString& shaderFile ) noexcept
	: SIRRenderProgram( device, shaderFile )
{
}

SIRGLProgram::~SIRGLProgram() noexcept
{
	if ( m_hProgramId != GL_INVALID_VALUE ) {
	}
}

#include "SIRGLMemoryObject.h"
#include "Renderer/Drivers/OpenGL/SIRGLDevice.h"

SIRGLMemoryObject::SIRGLMemoryObject( const SIRSharedPtr<SIRRenderDevice>& device ) noexcept
    : SIRRenderResource( device )
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	glDevice.createMemoryObjectsEXT( 1, &m_hMemoryObjectID );
}

SIRGLMemoryObject::~SIRGLMemoryObject() noexcept
{
	const SIRGLDevice& glDevice = getDevice<SIRGLDevice>();

	if ( m_hMemoryObjectID != GL_INVALID_VALUE ) {
		glDevice.deleteMemoryObjectsEXT( 1, &m_hMemoryObjectID );
	}
}

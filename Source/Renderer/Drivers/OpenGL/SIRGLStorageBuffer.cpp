#include "SIRGLStorageBuffer.h"

SIRGLStorageBuffer::SIRGLStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderBufferDescription_t& description ) noexcept
    : SIRRenderStorageBuffer( device, description, 1024 )
{
}

SIRGLStorageBuffer::~SIRGLStorageBuffer() noexcept
{
}

#include "GLBuffer.hpp"

GLBuffer::GLBuffer( void )
    : IRenderBuffer(), m_pMappedGPUBuffer( NULL )
{
    Init();
}

void GLBuffer::Init( void ) {
    nglCreateBuffers( 1, &m_nBufferId );  
}

void GLBuffer::Shutdown( void ) {
    nglDeleteBuffers( 1, &m_nBufferId );
}

void GLBuffer::Clear( void ) {
}

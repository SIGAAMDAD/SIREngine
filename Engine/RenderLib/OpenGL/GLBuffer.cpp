#include "GLBuffer.hpp"

GLBuffer::GLBuffer( gpuBufferType_t nType )
    : IRenderBuffer( nType ), m_pMappedGPUBuffer( NULL )
{
    Init();
}

void GLBuffer::Init( void ) {
    switch ( m_nType ) {
    case BUFFER_TYPE_VERTEX:
        m_nBufferTarget = GL_ARRAY_BUFFER;
        break;
    case BUFFER_TYPE_INDEX:
        m_nBufferTarget = GL_ELEMENT_ARRAY_BUFFER;
        break;
    case BUFFER_TYPE_UNIFORM:
        m_nBufferTarget = GL_UNIFORM_BUFFER;
        break;
    };

    nglCreateBuffers( 1, &m_nBufferId );
    nglBindBuffer( m_nBufferTarget, m_nBufferId );

    // allocate a default buffer size of 1 MB
    nglBufferData( m_nBufferTarget, 1*1024*1024, NULL, GL_DYNAMIC_DRAW );

    nglBindBuffer( m_nBufferTarget, 0 );
}

void GLBuffer::Shutdown( void ) {
    nglDeleteBuffers( 1, &m_nBufferId );
}

void GLBuffer::Clear( void ) {
}

void GLBuffer::Resize( uint64_t nSize )
{

}

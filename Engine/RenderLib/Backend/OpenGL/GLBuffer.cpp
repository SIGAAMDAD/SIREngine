#include "../RenderBuffer.h"
#include "GLBuffer.h"

GLBuffer::GLBuffer( GPUBufferType_t nType )
    : m_nBufferUsage( GL_DYNAMIC_DRAW )
{
    Init();
}

GLBuffer::GLBuffer( GPUBufferType_t nType, uint64_t nSize )
    : m_nBufferUsage( GL_DYNAMIC_DRAW )
{
    Init( nSize );
}

GLBuffer::~GLBuffer()
{
    Shutdown();
}

void GLBuffer::Copy( const IRenderBuffer& other )
{
    
}

void GLBuffer::Init( void )
{
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

    nglCreateBuffers( 1, &m_hBufferID );
    nglBindBuffer( m_nBufferTarget, m_hBufferID );

    // allocate a default buffer size of 1 MB
    nglBufferData( m_nBufferTarget, 1*1024*1024, NULL, GL_DYNAMIC_DRAW );

    nglBindBuffer( m_nBufferTarget, 0 );
}

void GLBuffer::Init( uint64_t nBytes )
{
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

    nglCreateBuffers( 1, &m_hBufferID );
    nglBindBuffer( m_nBufferTarget, m_hBufferID );
    nglBufferData( m_nBufferTarget, nBytes, NULL, GL_DYNAMIC_DRAW );
    nglBindBuffer( m_nBufferTarget, 0 );
}


void GLBuffer::Shutdown( void )
{
    nglDeleteBuffers( 1, &m_hBufferID );
}

void GLBuffer::Clear( void ) {
}

void GLBuffer::Resize( uint64_t nSize )
{

}

void GLBuffer::SwapData( GLPipelineSet_t *pSet )
{
    nglBindBuffer( m_nBufferTarget, m_hBufferID );

    if ( 1 ) {
        nglInvalidateBufferData( m_hBufferID );
        void *pMappedBuffer = nglMapBufferRange( m_nBufferTarget, 0, m_nBufferSize, GL_MAP_WRITE_BIT
            | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT );
        if ( pMappedBuffer ) {
//            memcpy( pMappedBuffer, pData, m_nBufferSize );
        }
        nglUnmapBuffer( m_nBufferTarget );
    } else {
        nglBufferData( m_nBufferTarget, m_nBufferSize, NULL, m_nBufferUsage );
//        nglBufferSubData( m_nBufferTarget, 0, m_nBufferSize, pData );
    }

    nglBindBuffer( m_nBufferTarget, 0 );
}
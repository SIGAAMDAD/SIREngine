#include "../RenderBuffer.h"
#include "GLBuffer.h"
#include "GLContext.h"

using namespace SIREngine::RenderLib::Backend::OpenGL;

extern bool32 g_bUseBufferDiscard;

GLBuffer::GLBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
    : IRenderBuffer( nType, nUsage )
{
    m_nBufferSize = nSize;
    Init( NULL, nSize );
}

GLBuffer::~GLBuffer()
{
    Shutdown();
}

void GLBuffer::Copy( const IRenderBuffer& other )
{
    const GLBuffer *pBuffer = dynamic_cast<const GLBuffer *>( eastl::addressof( other ) );

    SIRENGINE_LOG( "Copying GLBuffer 0x%lx to 0x%lx, size: %lu bytes...", (uintptr_t)(const void *)pBuffer, (uintptr_t)(void *)this,
        pBuffer->GetSize() );

    Resize( pBuffer->GetSize() );

    nglBindBuffer( GL_COPY_WRITE_BUFFER, m_hBufferID );
    nglBindBuffer( GL_COPY_READ_BUFFER, pBuffer->GetGLObject() );
    nglCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, pBuffer->GetSize() );
    nglBindBuffer( GL_COPY_READ_BUFFER, 0 );
    nglBindBuffer( GL_COPY_WRITE_BUFFER, 0 );

    m_nBufferSize = pBuffer->GetSize();

}

void GLBuffer::Init( const void *pBuffer, uint64_t nSize )
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
    default:
        SIRENGINE_ERROR( "Invalid gpu buffer type %i", (int)m_nType );
    };

    switch ( m_nUsage ) {
    case BufferUsage_Constant:
        m_nGLUsage = GL_STATIC_DRAW;
        break;
    case BufferUsage_Dynamic:
        m_nGLUsage = GL_DYNAMIC_DRAW;
        break;
    case BufferUsage_Stream:
        m_nGLUsage = GL_STREAM_DRAW;
        break;
    default:
        SIRENGINE_ERROR( "Invalid gpu buffer usage %i", (int)m_nUsage );
    };

    nglCreateBuffers( 1, &m_hBufferID );
    nglBindBuffer( m_nBufferTarget, m_hBufferID );

    if ( r_UseMappedBufferObjects.GetValue() ) {
        if ( m_nUsage == BufferUsage_Constant ) {
            nglBufferStorage( m_nBufferTarget, nSize, pBuffer, GL_MAP_WRITE_BIT );
            void *pMemory = nglMapBufferRange( m_nBufferTarget, 0, nSize, GL_MAP_WRITE_BIT );
            if ( pMemory ) {
                memcpy( pMemory, pBuffer, nSize );
            }
        } else {
            nglBufferStorage( m_nBufferTarget, nSize, pBuffer, GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT );
            if ( m_nUsage == BufferUsage_Stream ) {
                nglMapBufferRange( m_nBufferTarget, 0, nSize, GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT );
            }
        }
    }
    else {
        nglBufferData( m_nBufferTarget, nSize, pBuffer, m_nGLUsage );
    }

    nglBindBuffer( m_nBufferTarget, 0 );
}


void GLBuffer::Shutdown( void )
{
    if ( m_nUsage == BufferUsage_Stream ) {
        nglBindBuffer( m_nBufferTarget, m_hBufferID );
        nglUnmapBuffer( m_nBufferTarget );
        nglBindBuffer( m_nBufferTarget, 0 );
    }
    nglDeleteBuffers( 1, &m_hBufferID );
}

void GLBuffer::Clear( void ) {
    Bind();
    
    // set the buffer to null data
    nglBufferData( m_nBufferTarget, m_nBufferSize, NULL, m_nGLUsage );
}

void GLBuffer::Resize( uint64_t nSize )
{
    if ( nSize > m_nBufferSize ) {
        // copy the persistent data over
        if ( m_nUsage == BufferUsage_Stream ) {
            Bind();
            if ( r_UseMappedBufferObjects.GetValue() ) {
                nglUnmapBuffer( m_nBufferTarget );
            }
            uint32_t hOldBuffer = m_hBufferID;

            nglCreateBuffers( 1, &m_hBufferID );
            Init( NULL, nSize );

            nglBindBuffer( GL_COPY_READ_BUFFER, hOldBuffer );

            nglCopyBufferSubData( GL_COPY_READ_BUFFER, m_nBufferTarget, 0, 0, m_nBufferSize );
            nglBindBuffer( GL_COPY_READ_BUFFER, 0 );
            nglDeleteBuffers( 1, &hOldBuffer );

            Unbind();
        }
        else {
            Shutdown();
            Init( NULL, nSize );
        }
        m_nBufferSize = nSize;
    }
}

void GLBuffer::SwapData( GLPipelineSet_t *pSet )
{
    uint64_t nBytes;

    nglBindBuffer( m_nBufferTarget, m_hBufferID );

    nBytes = pSet->nPolyCount * sizeof( *pSet->pPolyList );
    Resize( nBytes );

    if ( g_bUseBufferDiscard ) {
        if ( r_UseMappedBufferObjects.GetValue() ) {
            nglInvalidateBufferData( m_hBufferID );
        }
        else {
            nglBufferData( m_nBufferTarget, m_nBufferSize, NULL, m_nGLUsage );
        }
    }
    if ( r_UseMappedBufferObjects.GetValue() ) {
        void *pMappedBuffer = nglMapBufferRange( m_nBufferTarget, 0, nBytes, GL_MAP_WRITE_BIT
            | GL_MAP_COHERENT_BIT | GL_MAP_PERSISTENT_BIT );
        if ( pMappedBuffer ) {
            memcpy( pMappedBuffer, pSet->pPolyList, nBytes );
        }
        nglUnmapBuffer( m_nBufferTarget );
    } else {
        nglBufferSubData( m_nBufferTarget, 0, nBytes, pSet->pPolyList );
    }

    nglBindBuffer( m_nBufferTarget, 0 );
}
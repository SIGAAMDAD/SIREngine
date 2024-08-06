#include "GLVertexArray.h"
#include "GLBuffer.h"

GLVertexArray::GLVertexArray( const RenderPipelineInputSet_t& createInfo )
{
    m_pVertexBuffer = new GLBuffer( BUFFER_TYPE_VERTEX, BufferUsage_Dynamic, 2*1024*1024 );
    m_pIndexBuffer = new GLBuffer( BUFFER_TYPE_INDEX, BufferUsage_Dynamic, 2*1024*1024 );

    m_pVertexAttribs = createInfo.pVertexAttribs;
    m_nAttribCount = createInfo.nAttribCount;

    nglGenVertexArrays( 1, &m_hVertexArrayID );
}

GLVertexArray::~GLVertexArray()
{
    nglDeleteVertexArrays( 1, &m_hVertexArrayID );

    delete m_pVertexBuffer;
    delete m_pIndexBuffer;
}
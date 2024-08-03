#include "GLVertexArray.h"
#include "GLBuffer.h"

GLVertexArray::GLVertexArray( const RenderPipelineInputSet_t& createInfo )
{
    m_pVertexBuffer = new GLBuffer( BUFFER_TYPE_VERTEX );
    m_pIndexBuffer = new GLBuffer( BUFFER_TYPE_INDEX );

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
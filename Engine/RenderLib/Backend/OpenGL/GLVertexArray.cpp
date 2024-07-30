#include "GLVertexArray.h"

GLVertexArray::GLVertexArray( void )
{
    m_pVertexBuffer = new GLBuffer( BUFFER_TYPE_VERTEX );
    m_pIndexBuffer = new GLBuffer( BUFFER_TYPE_INDEX );

    nglGenVertexArrays( 1, &m_hVertexArrayID );
}

GLVertexArray::~GLVertexArray()
{
    nglDeleteVertexArrays( 1, &m_hVertexArrayID );

    delete m_pVertexBuffer;
    delete m_pIndexBuffer;
}
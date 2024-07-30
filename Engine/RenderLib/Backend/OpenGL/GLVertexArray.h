#ifndef __GL_VERTEX_ARRAY_H__
#define __GL_VERTEX_ARRAY_H__

#pragma once

#include "../RenderVertexArray.h"
#include "GLBuffer.h"

class GLVertexArray : public IRenderVertexArray
{
public:
    GLVertexArray( void );
    virtual ~GLVertexArray() override;

    virtual void SetVertexAttribs( const VertexAttribInfo_t *vertexAttribs, uint64_t nAttribCount ) override;

    SIRENGINE_FORCEINLINE GLuint GetGLObject( void ) const
    { return m_hVertexArrayID; }
private:
    GLuint m_hVertexArrayID;
};

#endif
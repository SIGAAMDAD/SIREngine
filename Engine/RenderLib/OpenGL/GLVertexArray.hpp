#ifndef __GL_VERTEX_ARRAY_HPP__
#define __GL_VERTEX_ARRAY_HPP__

#pragma once

#include "GLBuffer.hpp"

class GLVertexArray
{
public:
    GLVertexArray( void );
    ~GLVertexArray();

    void SetVertexPointers( void );
private:
    GLBuffer m_VertexBuffer;
    GLBuffer m_IndexBuffer;
};

#endif
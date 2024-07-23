#ifndef __GL_BUFFER_HPP__
#define __GL_BUFFER_HPP__

#pragma once

#include <Engine/RenderLib/RenderBuffer.hpp>
#include "GLCommon.hpp"

class GLBuffer : public IRenderBuffer
{
public:
    GLBuffer( void );
    virtual ~GLBuffer() override;

    virtual void Init( void ) override;
    virtual void Shutdown( void ) override;

    virtual void Copy( const GLBuffer& other );
    virtual void Clear( void ) override;
private:
    void *m_pMappedGPUBuffer;
    GLuint m_nBufferId;
};

#endif
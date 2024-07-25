#ifndef __GL_BUFFER_HPP__
#define __GL_BUFFER_HPP__

#pragma once

#include <Engine/RenderLib/RenderBuffer.hpp>
#include "GLCommon.hpp"

class GLBuffer : public IRenderBuffer
{
public:
    GLBuffer( gpuBufferType_t nType );
    virtual ~GLBuffer() override;

    virtual void Init( void ) override;
    virtual void Init( uint64_t nItems ) override;
    virtual void Shutdown( void ) override;

    virtual void *GetBuffer( void ) override;
    virtual const void *GetBuffer( void ) const override;
    virtual uint64_t GetSize( void ) const override;

    virtual void Copy( const IRenderBuffer& other ) override;
    virtual void Clear( void ) override;
    virtual void Resize( uint64_t nSize ) override;
private:
    void *m_pMappedGPUBuffer;

    GLuint m_nBufferId;
    GLenum m_nBufferTarget;
};

#endif
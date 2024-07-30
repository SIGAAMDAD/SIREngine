#ifndef __GL_BUFFER_H__
#define __GL_BUFFER_H__

#pragma once

#include "../RenderBuffer.h"
#include "GLCommon.h"
#include "GLShaderPipeline.h"

class GLBuffer : public IRenderBuffer
{
public:
    GLBuffer( GPUBufferType_t nType );
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
    virtual void SwapData( GLPipelineSet_t *pSet );

    SIRENGINE_FORCEINLINE GLuint GetGLObject( void ) const
    { return m_hBufferID; }
    SIRENGINE_FORCEINLINE void Bind( void ) const
    { nglBindBuffer( m_nBufferTarget, m_hBufferID ); }
    SIRENGINE_FORCEINLINE void Unbind( void ) const
    { nglBindBuffer( m_nBufferTarget, 0 ); }
protected:
    GLuint m_hBufferID;
    GLenum m_nBufferTarget;
    GLenum m_nBufferUsage;
};

#endif
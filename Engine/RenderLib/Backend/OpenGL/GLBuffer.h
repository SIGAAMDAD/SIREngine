#ifndef __GL_BUFFER_H__
#define __GL_BUFFER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderBuffer.h"

class GLBuffer : public IRenderBuffer
{
public:
    GLBuffer( GPUBufferType_t nType );
    GLBuffer( GPUBufferType_t nType, uint64_t nSize );
    virtual ~GLBuffer() override;

    virtual void Init( void ) override;
    virtual void Init( uint64_t nItems ) override;
    virtual void Shutdown( void ) override;

    virtual void *GetBuffer( void ) override { return NULL; }
    virtual const void *GetBuffer( void ) const override { return NULL; }
    virtual uint64_t GetSize( void ) const override
    { return m_nBufferSize; }

    virtual void Copy( const IRenderBuffer& other ) override;
    virtual void Clear( void ) override;
    virtual void Resize( uint64_t nSize ) override;
    virtual void SwapData( GLPipelineSet_t *pSet );

    SIRENGINE_FORCEINLINE GLuint GetGLObject( void ) const
    { return m_hBufferID; }
    SIRENGINE_FORCEINLINE GLenum GetGLTarget( void ) const
    { return m_nBufferTarget; }
    SIRENGINE_FORCEINLINE GLenum GetGLUsage( void ) const
    { return m_nBufferUsage; }
    SIRENGINE_FORCEINLINE void Bind( void ) const
    { nglBindBuffer( m_nBufferTarget, m_hBufferID ); }
    SIRENGINE_FORCEINLINE void Unbind( void ) const
    { nglBindBuffer( m_nBufferTarget, 0 ); }
private:
    GLuint m_hBufferID;
    GLenum m_nBufferTarget;
    GLenum m_nBufferUsage;
};

#endif
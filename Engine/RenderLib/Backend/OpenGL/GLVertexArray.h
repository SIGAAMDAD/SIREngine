#ifndef __GL_VERTEX_ARRAY_H__
#define __GL_VERTEX_ARRAY_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "GLBuffer.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    class GLVertexArray
    {
    public:
        GLVertexArray( const RenderPipelineInputSet_t& createInfo );
        ~GLVertexArray();

        SIRENGINE_FORCEINLINE GLuint GetGLObject( void ) const
        { return m_hVertexArrayID; }

        SIRENGINE_FORCEINLINE GLBuffer *GetVertexBuffer( void )
        { return m_pVertexBuffer; }
        SIRENGINE_FORCEINLINE GLBuffer *GetIndexBuffer( void )
        { return m_pIndexBuffer; }

        SIRENGINE_FORCEINLINE void Bind( void )
        {
            nglBindVertexArray( m_hVertexArrayID );
            m_pVertexBuffer->Bind();
            m_pIndexBuffer->Bind();
        }
        SIRENGINE_FORCEINLINE void Unbind( void )
        {
            nglBindVertexArray( 0 );
            m_pVertexBuffer->Unbind();
            m_pIndexBuffer->Unbind();
        }
    private:
        GLBuffer *m_pVertexBuffer;
        GLBuffer *m_pIndexBuffer;
        const VertexAttribInfo_t *m_pVertexAttribs;
        uint64_t m_nAttribCount;

        GLuint m_hVertexArrayID;
    };
};

#endif
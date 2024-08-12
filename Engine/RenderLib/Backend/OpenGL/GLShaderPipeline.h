#ifndef __GL_SHADERPIPELINE_H__
#define __GL_SHADERPIPELINE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderShaderPipeline.h"
#include "GLProgram.h"
#include "GLVertexArray.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    class GLShaderPipeline : public Backend::IRenderShaderPipeline
    {
    public:
        GLShaderPipeline( void );
        virtual ~GLShaderPipeline() override;

        virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) override;
        virtual void SetShaderInputState( uint64_t nCacheID ) override;
        virtual void ResetPipelineState( void ) override;

        SIRENGINE_FORCEINLINE GLPipelineSet_t *GetPipelineData( void )
        { return &m_PipelineCache[ m_nUsedPipeline ]; }
        SIRENGINE_FORCEINLINE GLVertexArray *GetVertexArray( void )
        { return m_pPipelineArray; }
    private:
        void UpdateVertexPointers( const GLPipelineSet_t *pSet );

        CVector<GLPipelineSet_t> m_PipelineCache;

        GLProgram *m_pDefaultShader;
        GLVertexArray *m_pPipelineArray;

        uint32_t m_nEnabledAttribs;
    };
};

#endif
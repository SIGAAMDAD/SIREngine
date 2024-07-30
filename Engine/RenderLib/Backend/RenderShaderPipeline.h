#ifndef __RENDER_PIPELINE_H__
#define __RENDER_PIPELINE_H__

#pragma once

#include "RenderCommon.h"
#include "RenderVertexArray.h"
#include "RenderProgram.h"
#include "../DrawBuffer.h"

typedef struct {
    alignas( 16 ) glm::mat4 u_ModelViewProjection;
} VertexInput_Uniform_t;


typedef struct {
    UniformInfo_t *pUniforms;
} VertexRenderPass_t;

typedef struct {
    uint32_t nEnabledVertexAttributes;
    IRenderProgram *pShader;

    VertexRenderPass_t *pRenderPasses;
    uint64_t nRenderPassCount;
} VertexInputDescription_t;

class IRenderShaderPipeline
{
public:
    IRenderShaderPipeline( void );
    virtual ~IRenderShaderPipeline();

    virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) = 0;
    virtual void SetShaderInputState( uint64_t nCacheID ) = 0;
    virtual void ResetPipelineState( void ) = 0;

    SIRENGINE_FORCEINLINE uint64_t GetIndexCount( void ) const
    { return m_nIndexCount; }
    SIRENGINE_FORCEINLINE uint64_t GetCurrentPipeline( void ) const
    { return m_nUsedPipeline; }

    static const uint64_t INVALID_PIPELINE_CACHE_ID = SIRENGINE_UINT64_MAX;
protected:
    uint64_t m_nIndexCount;
    uint64_t m_nVertexCount;
    uint64_t m_nUsedPipeline;
};

extern const VertexAttribInfo_t szDefaultVertexAttribs[ NumAttribs ];
extern const UniformInfo_t szDefaultUniforms[ NumUniforms ];

#endif
#ifndef __RENDER_PIPELINE_H__
#define __RENDER_PIPELINE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/RenderLib/DrawBuffer.h>

class IRenderShaderPipeline
{
public:
    IRenderShaderPipeline( void )
        : m_nIndexCount( 0 ), m_nVertexCount( 0 ), m_nUsedPipeline( 0 )
    { }
    virtual ~IRenderShaderPipeline()
    { }

    virtual uint64_t AddVertexAttribSet( const VertexInputDescription_t& vertexInput ) = 0;
    virtual void SetShaderInputState( uint64_t nCacheID ) = 0;
    virtual void ResetPipelineState( void ) = 0;

    SIRENGINE_FORCEINLINE uint64_t GetIndexCount( void ) const
    { return m_nIndexCount; }
    SIRENGINE_FORCEINLINE uint64_t GetCurrentPipeline( void ) const
    { return m_nUsedPipeline; }

    static const uint64_t INVALID_PIPELINE_CACHE_ID = (uint64_t)-1;
protected:
    uint64_t m_nIndexCount;
    uint64_t m_nVertexCount;
    uint64_t m_nUsedPipeline;
};

extern const VertexAttribInfo_t szDefaultVertexAttribs[ NumAttribs ];
extern const UniformInfo_t szDefaultUniforms[ NumUniforms ];

#endif
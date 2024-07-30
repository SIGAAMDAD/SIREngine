#ifndef __RENDER_PROGRAM_H__
#define __RENDER_PROGRAM_H__

#pragma once

#include <Engine/RenderLib/RenderCommon.h>
#include "RenderShader.h"

typedef enum {

} ShaderProgramPass_t;

typedef struct {
    const char *pName;
    uint64_t nShaderPasses;
} RenderProgramInit_t;

typedef enum {
    Uniform_DiffuseMap,
    Uniform_NormalMap,
    Uniform_SpecularMap,

    Uniform_VertexInputBuffer,

    NumUniforms
} UniformNum_t;

typedef enum {
    UniformType_Buffer,
    UniformType_Vec2,
    UniformType_Vec3,
    UniformType_Vec4,
    UniformType_Sampler,

    NumUniformTypes
} UniformType_t;

typedef struct {
    const char *pName;
    UniformNum_t nIndex;
    UniformType_t nType;
    RenderShaderType_t nStage;
    uint64_t nSize;
} UniformInfo_t;

class IRenderProgram
{
public:
    IRenderProgram( void );
    virtual ~IRenderProgram();

    SIRENGINE_FORCEINLINE const CString& GetName( void ) const
    { return m_Name; }

    static IRenderProgram *Create( const RenderProgramInit_t& programInfo );

    virtual IRenderShader *GetVertexShader( void ) = 0;
    virtual IRenderShader *GetPixelShader( void ) = 0;

    virtual const IRenderShader *GetVertexShader( void ) const = 0;
    virtual const IRenderShader *GetPixelShader( void ) const = 0;

    virtual bool Load( void ) = 0;
protected:
    CString m_Name;

    IRenderShader *m_pVertexShader;
    IRenderShader *m_pFragmentShader;
};

#endif
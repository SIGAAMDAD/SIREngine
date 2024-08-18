#ifndef __RENDER_COMMON_H__
#define __RENDER_COMMON_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/Core/ResourceDef.h>
#include "RenderLib.h"

namespace SIREngine::RenderLib {
    typedef enum {
        ST_FRAGMENT,
        ST_VERTEX,
        ST_GEOMETRY,
        ST_TESSELATION,

        ST_INVALID
    } RenderShaderType_t;
    #define ST_PIXLE ST_FRAGMENT

    typedef enum {
        Detail_VeryLow,
        Detail_Low,
        Detail_Medium,
        Detail_High,
        Detail_VeryHigh,
        Detail_Extreme
    } RenderDetail_t;

    typedef enum {
        BufferUsage_Stream,
        BufferUsage_Dynamic,
        BufferUsage_Constant
    } GPUBufferUsage_t;

    typedef enum {
        BUFFER_TYPE_VERTEX,
        BUFFER_TYPE_INDEX,
        BUFFER_TYPE_UNIFORM,
        BUFFER_TYPE_TEXTURE,

        BUFFER_TYPE_NONE
    } GPUBufferType_t;

    typedef enum {

    } ShaderProgramPass_t;


    typedef enum {
        Uniform_DiffuseMap,
        Uniform_NormalMap,
        Uniform_SpecularMap,
        Uniform_LightBuffer,

        Uniform_VertexInput,

        NumUniforms
    } UniformNum_t;

    typedef enum {
        UniformType_StaticBuffer,
        UniformType_DynamicBuffer,
        UniformType_Sampler,

        NumUniformTypes
    } UniformType_t;

    typedef enum {
        Attrib_Position,
        Attrib_TexCoords,
        Attrib_Color,
        Attrib_WorldPos,
        Attrib_Normal,

        NumAttribs
    } VertexAttribName_t;

    typedef enum {
        AttribBit_Position  = SIRENGINE_BIT( Attrib_Position ),
        AttribBit_WorldPos  = SIRENGINE_BIT( Attrib_WorldPos ),
        AttribBit_TexCoords = SIRENGINE_BIT( Attrib_TexCoords ),
        AttribBit_Color     = SIRENGINE_BIT( Attrib_Color ),
        AttribBit_Normal    = SIRENGINE_BIT( Attrib_Normal ),

        AttribBits =
            AttribBit_Position |
            AttribBit_WorldPos |
            AttribBit_TexCoords |
            AttribBit_Color |
            AttribBit_Normal
    } VertexAttribBits_t;

    typedef enum {
        VTA_INT,
        VTA_FLOAT,

        VTA_INT64,
        VTA_FLOAT64,

        VTA_VEC2,
        VTA_VEC3,
        VTA_VEC4,

        VTA_COLOR16,
        VTA_NORMAL16,

        NUMATTRIBTYPES
    } VertexAttribType_t;

    typedef enum {
        TF_RGBA,
        TF_RGB,

        TF_R, // specular maps
        TF_SRGB, // gamma adjusted texture

        // framebuffer depthbuffer formats
        TF_DEPTH,
        TF_DEPTH_STENCIL,

        // compressed texture formats

        // HDR texture formats
        TF_RGBA16F,
        TF_RGBA32F,

        TF_INVALID
    } TextureImageFormat_t;

    typedef struct {
        FileSystem::CFilePath filePath;
        bool32 bIsGPUOnly;

        // for framebuffer images
        uint32_t nWidth;
        uint32_t nHeight;
        uint32_t nChannels;
        TextureImageFormat_t nFormat;
    } TextureInit_t;

    typedef struct RenderProgramInit {
        const char *pName;
        uint64_t nShaderPasses;
    } RenderProgramInit_t;

    typedef struct {
        const char *pName;
        UniformNum_t nIndex;
        UniformType_t nType;
        RenderShaderType_t nStage;
        uint64_t nSize;
        uint64_t nVariables;
    } UniformInfo_t;

    typedef struct {
        const char *pName;
        uintptr_t nStride;
        uintptr_t nOffset;
        uint32_t nShaderBinding;
        VertexAttribType_t nType;
        bool32 bEnabled;
        bool32 bNormalized;
    } VertexAttribInfo_t;

    namespace Backend { class IRenderProgram; };

    typedef struct {
        const VertexAttribInfo_t *pVertexAttribs;
        uint64_t nAttribCount;

        Backend::IRenderProgram *pShader;
    } RenderPipelineInputSet_t;

    typedef struct {
        const char *pName;
        RenderShaderType_t nType;
    } RenderShaderInit_t;


    typedef struct {
        UniformInfo_t *pUniforms;
    } VertexRenderPass_t;

    typedef struct {
        uint32_t nEnabledVertexAttributes;
        Backend::IRenderProgram *pShader;

        const UniformInfo_t *pUniformList;
        uint64_t nUniformCount;

        const VertexRenderPass_t *pRenderPasses;
        uint64_t nRenderPassCount;
    } VertexInputDescription_t;

    typedef struct {
        glm::mat4 u_ModelViewProjection;
    } VertexInput_Uniform_t;

    typedef struct {
        alignas( 16 ) CVec4 color;
        alignas( 16 ) CUVec2 origin;
        alignas( 16 ) float brightness;
        alignas( 16 ) float range;
        alignas( 16 ) float linear;
        alignas( 16 ) float quadratic;
        alignas( 16 ) float constant;
        alignas( 16 ) int type;
    } LightBuffer_Uniform_t;
};

#endif
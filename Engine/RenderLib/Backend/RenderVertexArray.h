#ifndef __RENDER_VERTEXARRAY_H__
#define __RENDER_VERTEXARRAY_H__

#pragma once

#include "../RenderCommon.h"

typedef enum {
    Attrib_Position,
    Attrib_WorldPos,
    Attrib_TexCoords,
    Attrib_Color,
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

typedef struct {
    const char *pName;
    uintptr_t nStride;
    uintptr_t nOffset;
    uint32_t nShaderBinding;
    VertexAttribType_t nType;
    bool32 bEnabled;
    bool32 bNormalized;
} VertexAttribInfo_t;

typedef struct {
    const VertexAttribInfo_t *pVertexAttribs;
    uint64_t nAttribCount;

    IRenderProgram *pShader;
} RenderPipelineInputSet_t;

class IRenderVertexArray
{
public:
    IRenderVertexArray( void )
    { }
    virtual ~IRenderVertexArray()
    { }

    static IRenderVertexArray *Create( const RenderPipelineInputSet_t& createInfo );

    virtual IRenderBuffer *GetVertexBuffer( void ) = 0;
    virtual IRenderBuffer *GetIndexBuffer( void ) = 0;
    virtual const IRenderBuffer *GetVertexBuffer( void ) const = 0;
    virtual const IRenderBuffer *GetIndexBuffer( void ) const = 0;

    SIRENGINE_FORCEINLINE const VertexAttribInfo_t *GetAttribs( void ) const
    {
        return m_pVertexAttribs;
    }
    SIRENGINE_FORCEINLINE uint64_t GetAttribCount( void ) const
    {
        return m_nAttribCount;
    }

    virtual void Init( void ) = 0;
    virtual void SetVertexAttribs( const VertexAttribInfo_t *vertexAttribs, uint64_t nAttribCount ) = 0;
protected:
    CString m_Name;
    IRenderBuffer *m_pVertexBuffer;
    IRenderBuffer *m_pIndexBuffer;

    const VertexAttribInfo_t *m_pVertexAttribs;
    uint64_t m_nAttribCount;
};

#endif
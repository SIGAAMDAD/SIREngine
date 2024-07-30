#ifndef __RENDER_SHADER_H__
#define __RENDER_SHADER_H__

#pragma once

#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/Core/FileSystem/MemoryFile.h>

typedef enum {
    ST_VERTEX,
    ST_FRAGMENT,
    ST_GEOMETRY,
    ST_TESSELATION,

    ST_PIXEL = ST_FRAGMENT,

    ST_INVALID
} RenderShaderType_t;

typedef struct {
    const char *pName;
    RenderShaderType_t nType;
} RenderShaderInit_t;

class IRenderShader : public CResourceDef
{
public:
    IRenderShader( void )
        : m_nType( ST_INVALID )
    { }
    virtual ~IRenderShader()
    { }

    static IRenderShader *Create( const RenderShaderInit_t& shaderInit );

    virtual RenderShaderType_t GetType( void ) const = 0;
    virtual const char *GetName( void ) const = 0;
protected:
    virtual bool Load( const char *pszFilePath ) = 0;

    RenderShaderType_t m_nType;
};

#endif
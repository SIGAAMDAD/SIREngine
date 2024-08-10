#ifndef __GL_SHADER_H__
#define __GL_SHADER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderShader.h"

class GLShader : public IRenderShader
{
public:
    GLShader( const RenderShaderInit_t& shaderInfo );
    virtual ~GLShader() override;

    virtual const char *GetName( void ) const override { return m_szName; }
    virtual bool IsValid( void ) const override { return false; }
    virtual void Reload( void ) override { }
    virtual void Release( void ) override { }

    virtual RenderShaderType_t GetType( void ) const override { return m_nType; }
private:
    virtual bool Load( const char *pszFilePath ) override { return false; }

    GLuint m_hShaderID;
};

#endif
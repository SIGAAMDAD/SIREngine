#ifndef __GL_SHADER_H__
#define __GL_SHADER_H__

#pragma once

#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"

class GLShader : public IRenderShader
{
public:
    GLShader( const RenderShaderInit_t& shaderInfo );
    virtual ~GLShader() override;

    virtual const char *GetName( void ) const override;
    virtual bool IsValid( void ) const override;
    virtual void Reload( void ) override;
    virtual void Release( void ) override;

    virtual RenderShaderType_t GetType( void ) const override;
    virtual const char *GetName( void ) const override;
private:
    virtual bool Load( const char *pszFilePath ) override;

    GLuint m_hShaderID;
};

#endif
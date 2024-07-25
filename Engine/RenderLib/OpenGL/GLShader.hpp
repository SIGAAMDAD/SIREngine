#ifndef __GL_SHADER_HPP__
#define __GL_SHADER_HPP__

#pragma once

#include <Engine/RenderLib/RenderShader.hpp>

class GLShader : public IRenderShader
{
public:
    GLShader( const char *pszShaderName );
    virtual ~GLShader() override;
private:
    virtual bool Load( const char *pszFilePath ) override;

    GLuint m_nProgramID;
};

#endif
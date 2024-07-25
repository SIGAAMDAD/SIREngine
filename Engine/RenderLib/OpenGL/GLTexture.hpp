#ifndef __GL_TEXTURE_HPP__
#define __GL_TEXTURE_HPP__

#pragma once

#include "GLCommon.hpp"
#include <Engine/RenderLib/RenderTexture.hpp>

class GLTexture : public IRenderTexture
{
public:
    GLTexture( void );
    virtual ~GLTexture() override;

private:
    virtual void Upload( void ) override;

    GLuint m_nTextureID;
};

#endif
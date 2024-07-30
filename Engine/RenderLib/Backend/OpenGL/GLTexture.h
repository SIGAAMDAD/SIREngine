#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#pragma once

#include "GLCommon.h"
#include <Engine/RenderLib/RenderTexture.h>

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
#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderTexture.h"
#include "../ImageLoader.h"

class GLTexture : public IRenderTexture
{
public:
    GLTexture( const TextureInit_t& textureInfo );
    virtual ~GLTexture();

    virtual void StreamBuffer( void ) override;
private:
    virtual void Upload( const TextureInit_t& textureInfo ) override;

    GLuint m_nTextureID;
    GLuint m_hBufferID[2];
    GLint m_nBufferIndex;

    GLenum m_nGLTextureFormat;

    CImageLoader m_ImageData;
};

#endif
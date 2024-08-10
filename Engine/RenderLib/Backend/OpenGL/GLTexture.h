#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderTexture.h"
#include "../ImageLoader.h"
#include <EASTL/bonus/lru_cache.h>

class GLTexture : public IRenderTexture
{
public:
    GLTexture( const TextureInit_t& textureInfo );
    virtual ~GLTexture();

    void EvictGLResource( void );

    virtual void StreamBuffer( void ) override;
private:
    virtual void Upload( const TextureInit_t& textureInfo ) override;

    bool CanBeEvicted( void ) const;

    GLuint m_hBufferID[2];
    GLuint m_nTextureID;
    GLint m_nBufferIndex;

    bool32 m_bInGPUMemory;
    bool32 m_bCanCreateAsEvicted;
    uint64_t m_nFrameLastRendered;

    GLenum m_nGLTextureFormat;

    CImageLoader m_ImageData;
};

#endif
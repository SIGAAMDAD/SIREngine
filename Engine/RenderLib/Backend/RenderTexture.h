#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#pragma once

#include <Engine/RenderLib/RenderCommon.h>
#include "ImageLoader.h"

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
    CFilePath filePath;
    bool32 bIsGPUOnly;

    // for framebuffer images
    uint32_t nWidth;
    uint32_t nHeight;
    uint32_t nChannels;
    TextureImageFormat_t nFormat;
} TextureInit_t;

class IRenderTexture : public CResourceDef
{
public:
    IRenderTexture( void )
        : CResourceDef(), m_nWidth( 0 ), m_nHeight( 0 ), m_ImageFormat( TF_INVALID )
    { }
    virtual ~IRenderTexture();

    static IRenderTexture *Create( const TextureInit_t& textureInfo );

    virtual const char *GetPath( void ) const = 0;

    virtual void *GetBuffer( void ) = 0;
    virtual const void *GetBuffer( void ) const = 0;
    virtual uint64_t GetSize( void ) const = 0;
protected:
    virtual void Upload( const TextureInit_t& textureInfo ) = 0;

    uint32_t m_nWidth;
    uint32_t m_nHeight;

    TextureImageFormat_t m_ImageFormat;
};

#endif
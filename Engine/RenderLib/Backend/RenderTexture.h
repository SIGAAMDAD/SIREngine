#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/RenderLib/RenderCommon.h>

namespace SIREngine::RenderLib::Backend {
    class IRenderTexture
    {
    public:
        IRenderTexture( void )
            : m_nWidth( 0 ), m_nHeight( 0 ), m_ImageFormat( TF_INVALID )
        { }
        virtual ~IRenderTexture()
        { }

        static IRenderTexture *Create( const TextureInit_t& textureInfo );

        virtual void StreamBuffer( void )
        { }

        /**
         * @brief Loads the texture's data into memory on the SyncedLoaderThread
         * @param in const TextureInit_t& textureInfo
         * @return
        */
        virtual void LoadFile( const TextureInit_t& textureInfo )
        { }
        virtual void Upload( void )
        { }
    protected:
        uint32_t m_nWidth;
        uint32_t m_nHeight;

        TextureImageFormat_t m_ImageFormat;
    };

    extern CVar<int32_t> r_TextureStreamingBudget;
    extern CVar<bool32> r_UseHDRTextures;
};

#endif
#ifndef __RENDER_TEXTURE_H__
#define __RENDER_TEXTURE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/RenderLib/RenderCommon.h>

namespace SIREngine::RenderLib::Backend {
    class IRenderTexture : public CResourceDef
    {
    public:
        IRenderTexture( void )
            : CResourceDef(), m_nWidth( 0 ), m_nHeight( 0 ), m_ImageFormat( TF_INVALID )
        { }
        virtual ~IRenderTexture()
        { }

        static IRenderTexture *Create( const TextureInit_t& textureInfo );

        virtual void StreamBuffer( void )
        { }

        virtual const char *GetName( void ) const override { return m_szName; }
        virtual bool IsValid( void ) const override { return false; }
        virtual void Reload( void ) override { }
        virtual void Release( void ) override { }
    protected:
        virtual void Upload( const TextureInit_t& textureInfo )
        { }

        uint32_t m_nWidth;
        uint32_t m_nHeight;

        TextureImageFormat_t m_ImageFormat;
    };

    extern CVar<int32_t> r_TextureStreamingBudget;
    extern CVar<bool32> r_UseHDRTextures;
};

#endif
#ifndef __VK_TEXTURE_H__
#define __VK_TEXTURE_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderTexture.h"
#include "../ImageLoader.h"

namespace SIREngine::RenderLib::Backend::Vulkan {
    class VKTexture : public Backend::IRenderTexture
    {
    public:
        VKTexture( const TextureInit_t& textureInfo );
        virtual ~VKTexture();

        virtual void StreamBuffer( void ) override;

		virtual void Upload( const TextureInit_t& textureInfo ) override;
    private:
        VkImage m_hImage;
        VmaAllocation m_hImageMemory;
        bool32 m_bIsGPUOnly;
        TextureImageFormat_t m_nImageFormat;

        CImageLoader m_ImageData;
    };
};

#endif
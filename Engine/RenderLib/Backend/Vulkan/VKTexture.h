#ifndef __VK_TEXTURE_H__
#define __VK_TEXTURE_H__

#pragma once

#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"

class VKTexture : public IRenderTexture
{
public:
    VKTexture( const TextureInit_t& textureInfo );
    virtual ~VKTexture() override;
private:
    virtual void Upload( const TextureInit_t& textureInfo ) override;

    VkImage m_hImage;
    VmaAllocation m_hImageMemory;
};

#endif
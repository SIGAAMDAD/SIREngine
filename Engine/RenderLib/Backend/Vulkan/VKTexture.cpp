#include "VKBuffer.h"
#include "VKTexture.h"

VKTexture::VKTexture( const TextureInit_t& textureInfo )
{
    Upload( textureInfo );
}

VKTexture::~VKTexture()
{
    vmaDestroyImage( g_pVKContext->GetAllocator(), m_hImage, m_hImageMemory );
}

void VKTexture::Upload( const TextureInit_t& textureInfo )
{
    VkImageCreateInfo imageInfo;
    memset( &imageInfo, 0, sizeof( imageInfo ) );
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = textureInfo.nWidth;
    imageInfo.extent.height = textureInfo.nHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if ( textureInfo.bIsGPUOnly ) {
        VmaAllocationCreateInfo allocInfo;
        memset( &allocInfo, 0, sizeof( allocInfo ) );
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;

        if ( textureInfo.nFormat == TF_DEPTH || textureInfo.nFormat == TF_DEPTH_STENCIL ) {
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        } else {
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }

        switch ( textureInfo.nFormat ) {
        case TF_R:
            imageInfo.format = VK_FORMAT_R8_SRGB;
            break;
        case TF_RGB:
            imageInfo.format = VK_FORMAT_R8G8B8_SRGB;
            break;
        case TF_RGBA:
            imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            break;
        case TF_RGBA16F:
            imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            break;
        case TF_RGBA32F:
            imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            break;
        case TF_DEPTH: {
            const CStaticArray<VkFormat, 2> szFormats = {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };
            imageInfo.format = g_pVKContext->GetSupportedFormat( szFormats.GetBuffer(), szFormats.Size(),
                VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
            break; }
        case TF_DEPTH_STENCIL: {
            const CStaticArray<VkFormat, 2> szFormats = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
            };
            imageInfo.format = g_pVKContext->GetSupportedFormat( szFormats.GetBuffer(), szFormats.Size(),
                VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
            break; }
        };

        if ( vmaCreateImage( g_pVKContext->GetAllocator(), &imageInfo, &allocInfo, &m_hImage,
            &m_hImageMemory, NULL ) != VK_SUCCESS )
        {

        }
    }
    else {
        CImageLoader image( textureInfo.filePath );

        m_nWidth = image.GetWidth();
        m_nHeight = image.GetHeight();

        VmaAllocationCreateInfo allocInfo;
        memset( &allocInfo, 0, sizeof( allocInfo ) );
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        allocInfo.priority = 1.0f;

        if ( image.GetChannels() == 3 ) {
            imageInfo.format = VK_FORMAT_R8G8B8_SRGB;
        } else {
            imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        }

        VmaAllocationInfo allocData;

        vmaCreateImage( g_pVKContext->GetAllocator(), &imageInfo, &allocInfo,
            &m_hImage, &m_hImageMemory, &allocData );
        memcpy( allocData.pMappedData, image.GetBuffer(), image.GetSize() );
        vmaUnmapMemory( g_pVKContext->GetAllocator(), m_hImageMemory );
    }
}
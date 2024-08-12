#include "VKFramebuffer.h"
#include "VKContext.h"

using namespace SIREngine::RenderLib::Backend::Vulkan;

VKFramebuffer::VKFramebuffer( const FramebufferInfo_t& info )
{
    uint64_t i;
    TextureInit_t textureInfo;

    m_Attachments.resize( info.nAttachmentCount );

    textureInfo.nWidth = info.nWidth;
    textureInfo.nHeight = info.nHeight;
    textureInfo.bIsGPUOnly = true;

    for ( i = 0; i < info.nAttachmentCount; i++ ) {
        VkImageCreateInfo imageInfo;
        memset( &imageInfo, 0, sizeof( imageInfo ) );
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

        VkImageViewCreateInfo imageViewInfo;
        memset( &imageViewInfo, 0, sizeof( imageViewInfo ) );
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

        switch ( info.pAttachments[i] ) {
        case FBA_BLOOMBUFFER:
            
        case FBA_COLORBUFFER:
        case FBA_DEPTHBUFFER:
            imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
        };
    }

    VkFramebufferCreateInfo framebufferInfo;
    memset( &framebufferInfo, 0, sizeof( framebufferInfo ) );
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = NULL;
    framebufferInfo.attachmentCount = m_Attachments.size();
    framebufferInfo.pAttachments = m_Attachments.data();

    VK_CALL( vkCreateFramebuffer( g_pVKContext->GetDevice(), &framebufferInfo, g_pVKContext->GetAllocationCallbacks(), &m_hFramebuffer ) );
}

VKFramebuffer::~VKFramebuffer()
{
    if ( m_hFramebuffer ) {
        vkDestroyFramebuffer( g_pVKContext->GetDevice(), m_hFramebuffer, g_pVKContext->GetAllocationCallbacks() );
    }
}

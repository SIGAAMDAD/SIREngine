#include "SIRVulkanFramebuffer.h"
#include <vulkan/vulkan_core.h>

static const VkImageView *createImageViews( const sirRenderFramebufferDescription_t& description )
{
	static const VkImageView *pImageViews = (const VkImageView *)alloca( sizeof( *pImageViews ) * description.nColorCount );
	for ( uint32_t i = 0; i < description.nColorCount; i++ ) {
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	}
}

SIRVulkanFramebuffer::SIRVulkanFramebuffer( const SIRSharedPtr<SIRRenderDevice>& device, const sirRenderFramebufferDescription_t& description ) noexcept
    : SIRRenderFramebuffer( device, description ),
      m_hFramebuffer( VK_NULL_HANDLE )
{


	VkFramebufferCreateInfo framebufferCreateInfo{};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
}

SIRVulkanFramebuffer::~SIRVulkanFramebuffer() noexcept
{
	if ( m_hFramebuffer != VK_NULL_HANDLE ) {
	}
}

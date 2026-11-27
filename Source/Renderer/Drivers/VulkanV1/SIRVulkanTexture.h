#ifndef __SIR_VULKAN_TEXTURE_H__
#define __SIR_VULKAN_TEXTURE_H__

#include <Renderer/RHI/SIRRenderTexture.h>
#include <Renderer/RHI/SIRRenderTextureView.h>
#include <Renderer/RHI/SIRRenderSampler.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

class SIRVulkanTexture final : public SIRRenderTexture
{
public:
	SIRVulkanTexture( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderTextureDesc& description ) noexcept;
	~SIRVulkanTexture() noexcept override;
	SIRRenderResult getSubresourceLayout( const SIRRenderSubresourceRange&, SIRRenderBufferRange *pLayout ) const noexcept override;
	const sirVulkanImage_t& getNativeImage( void ) const noexcept { return m_Native; }
	bool isValid( void ) const noexcept { return m_Native.hImage != VK_NULL_HANDLE; }

private:
	sirVulkanImage_t m_Native{};
};

class SIRVulkanTextureView final : public SIRRenderTextureView
{
public:
	SIRVulkanTextureView( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRRenderTexture>& texture,
	                      const SIRRenderViewDesc& description ) noexcept;
	~SIRVulkanTextureView() noexcept override;
	VkImageView getNativeView( void ) const noexcept { return m_hView.hView; }
	SIRRenderHandle getHandle( void ) const noexcept { return m_Handle; }

private:
	sirVulkanImageView_t m_hView{};
	SIRRenderHandle m_Handle{};
};

class SIRVulkanSampler final : public SIRRenderSampler
{
public:
	SIRVulkanSampler( const SIRSharedPtr<SIRRenderDevice>& device, const SIRRenderSamplerDesc& description ) noexcept;
	~SIRVulkanSampler() noexcept override;
	VkSampler getNativeSampler( void ) const noexcept { return m_hSampler.hSampler; }

private:
	sirVulkanSampler_t m_hSampler{};
};

#endif

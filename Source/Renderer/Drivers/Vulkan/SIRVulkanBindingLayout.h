#ifndef __SIR_VULKAN_BINDING_LAYOUT_H__
#define __SIR_VULKAN_BINDING_LAYOUT_H__

#include <Renderer/RHI/SIRRenderBindingLayout.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>

class SIRVulkanBindingLayout final : public SIRRenderBindingLayout
{
public:
	SIRVulkanBindingLayout( const SIRSharedPtr<SIRRenderDevice>& device, SIRVector<SIRRenderBinding>&& bindings,
	                        uint64_t nSignatureHash = 0 ) noexcept;
	~SIRVulkanBindingLayout() noexcept override;

	VkDescriptorSetLayout getNativeLayout() const noexcept { return m_hLayout; }
	bool isValid() const noexcept { return m_hLayout != VK_NULL_HANDLE; }

private:
	VkDescriptorSetLayout m_hLayout{ VK_NULL_HANDLE };
};

#endif

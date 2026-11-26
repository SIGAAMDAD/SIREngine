#ifndef __SIR_VULKAN_BINDING_SET_H__
#define __SIR_VULKAN_BINDING_SET_H__

#include <Renderer/RHI/SIRRenderBindingSet.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanBindingLayout.h>

struct SIRVulkanDescriptorWrite
{
	SIRSharedPtr<SIRRenderResource> pResource;
	uint32_t nBinding{};
	uint32_t nArrayIndex{};
	uint64_t nOffsetBytes{};
	uint64_t nRangeBytes{};
};

class SIRVulkanBindingSet final : public SIRRenderBindingSet
{
public:
	SIRVulkanBindingSet( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRRenderBindingLayout>& layout,
	                     SIRVector<SIRRenderBinding>&& bindings ) noexcept;
	~SIRVulkanBindingSet() noexcept override;

	VkDescriptorSet getNativeSet() const noexcept { return m_Native.hSet; }
	VkDescriptorSetLayout getNativeLayout() const noexcept { return m_Native.hSetLayout; }
	const sirVulkanDescriptorSet_t& getNativeDescriptorSet() const noexcept { return m_Native; }
	bool isValid() const noexcept { return m_Native.hSet != VK_NULL_HANDLE; }
	SIRVulkanResult update( SIRSpan<const SIRVulkanDescriptorWrite> writes ) noexcept;

private:
	struct Reference
	{
		uint32_t nBinding{};
		uint32_t nArrayIndex{};
		SIRSharedPtr<SIRRenderResource> pResource;
	};

	sirVulkanDescriptorSet_t m_Native{};
	SIRVector<Reference> m_References;
};

#endif

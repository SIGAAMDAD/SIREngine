#include <Renderer/Drivers/Vulkan/SIRVulkanBindingLayout.h>

SIRVulkanBindingLayout::SIRVulkanBindingLayout( const SIRSharedPtr<SIRRenderDevice>& device, SIRVector<SIRRenderBindingDesc>&& bindings,
                                                uint64_t nSignatureHash ) noexcept
    : SIRRenderBindingLayout( device, static_cast<SIRVector<SIRRenderBindingDesc>&&>( bindings ), nSignatureHash )
{
	if ( device.get() != nullptr && device->getBackend() == SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ) {
		static_cast<void>( getDevice<SIRVulkanDevice>().createDescriptorSetLayout( m_Bindings.data(), m_Bindings.getSize(), &m_hLayout ) );
	}
}

SIRVulkanBindingLayout::~SIRVulkanBindingLayout() noexcept
{
	if ( getDevice().get() != nullptr && m_hLayout != VK_NULL_HANDLE ) {
		static_cast<void>( getDevice<SIRVulkanDevice>().destroyDescriptorSetLayout( &m_hLayout ) );
	}
}

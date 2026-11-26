#include <Renderer/Drivers/Vulkan/SIRVulkanBindingLayout.h>

SIRVulkanBindingLayout::SIRVulkanBindingLayout( const SIRSharedPtr<SIRRenderDevice>& device, SIRVector<SIRRenderBinding>&& bindings,
                                                uint64_t nSignatureHash ) noexcept
    : SIRRenderBindingLayout( device, static_cast<SIRVector<SIRRenderBinding>&&>( bindings ), nSignatureHash )
{
	SIRVulkanDevice *pDevice = dynamic_cast<SIRVulkanDevice *>( device.get() );
	if ( pDevice != nullptr ) {
		pDevice->createDescriptorSetLayout( m_Bindings.data(), m_Bindings.getSize(), &m_hLayout );
	}
}

SIRVulkanBindingLayout::~SIRVulkanBindingLayout() noexcept
{
	SIRVulkanDevice *pDevice = dynamic_cast<SIRVulkanDevice *>( getDevice().get() );
	if ( pDevice != nullptr && m_hLayout != VK_NULL_HANDLE ) {
		pDevice->waitIdle();
		pDevice->destroyDescriptorSetLayout( &m_hLayout );
	}
}

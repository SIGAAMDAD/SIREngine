#include "SIRVulkanProgram.h"
#include "SIRVulkanDevice.h"

SIRVulkanProgram::SIRVulkanProgram( const SIRSharedPtr<SIRRenderDevice>& device, SIRSpan<const SIRSharedPtr<SIRVulkanShader>> shaders,
                                    const sirContainerAllocator_t *pAllocator ) noexcept
    : SIRRenderProgram( device, pAllocator )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ||
	     shaders.getData() == nullptr || shaders.getSize() == 0 ) {
		return;
	}
	for ( size_t i = 0; i < shaders.getSize(); ++i ) {
		const SIRSharedPtr<SIRVulkanShader>& shader = shaders[i];
		if ( !shader.isValid() || !shader->isValid() || shader->getDevice().get() != device.get() ||
		     ( m_nStageMask & static_cast<uint32_t>( shader->getStage() ) ) != 0 ||
		     m_Shaders.tryEmplaceBack( shader ) != SIRContainerResult::SIR_CONTAINER_OK ) {
			m_Shaders.clear();
			m_nStageMask = 0;
			return;
		}
		m_nStageMask |= static_cast<uint32_t>( shader->getStage() );
	}
	m_bValid = true;
}

const SIRVulkanShader *SIRVulkanProgram::getShader( SIRRenderShaderStage eStage ) const noexcept
{
	if ( !m_bValid ) {
		return nullptr;
	}
	for ( uint32_t i = 0; i < m_Shaders.getSize(); ++i ) {
		if ( m_Shaders[i]->getStage() == eStage ) {
			return static_cast<const SIRVulkanShader *>( m_Shaders[i].get() );
		}
	}
	return nullptr;
}

bool SIRVulkanProgram::isValid( void ) const noexcept
{
	return m_bValid;
}

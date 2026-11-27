#include "SIRVulkanPipeline.h"

SIRVulkanPipeline::SIRVulkanPipeline( const SIRSharedPtr<SIRRenderDevice>& device, const SIRSharedPtr<SIRVulkanBindingLayout>& layout,
                                      const SIRSharedPtr<SIRVulkanProgram>& program, const SIRRenderPipelineDesc& description ) noexcept
    : SIRRenderPipeline( device, description.eType, layout, program )
{
	if ( device.get() == nullptr || device->getBackend() != SIRRenderBackend::SIR_RENDER_BACKEND_VULKAN ||
	     !program.isValid() || !program->isValid() || program->getDevice().get() != device.get() ||
	     ( layout.isValid() && ( !layout->isValid() || layout->getDevice().get() != device.get() ) ) ) {
		return;
	}

	SIRVulkanDevice& nativeDevice = getDevice<SIRVulkanDevice>();
	const VkDescriptorSetLayout hSetLayout = layout.isValid() ? layout->getNativeLayout() : VK_NULL_HANDLE;
	if ( description.eType == SIRRenderPipelineType::SIR_RENDER_PIPELINE_COMPUTE ) {
		const SIRVulkanShader *pCompute = program->getShader( SIRRenderShaderStage::SIR_RENDER_STAGE_COMPUTE );
		if ( pCompute == nullptr || program->getShaders().getSize() != 1 ) {
			return;
		}
		static_cast<void>( nativeDevice.createComputePipeline( pCompute->getNativeModule(), pCompute->getEntryPoint(),
		                                                   hSetLayout == VK_NULL_HANDLE ? nullptr : &hSetLayout,
		                                                   hSetLayout == VK_NULL_HANDLE ? 0U : 1U, &m_Native ) );
		return;
	}
	if ( description.eType != SIRRenderPipelineType::SIR_RENDER_PIPELINE_GRAPHICS || program->getShaders().getSize() != 2 ) {
		return;
	}
	static_cast<void>( nativeDevice.createRasterPipeline( description, *program, hSetLayout, &m_Native ) );
}

SIRVulkanPipeline::~SIRVulkanPipeline() noexcept
{
	if ( m_Native.hPipeline == VK_NULL_HANDLE || getDevice().get() == nullptr ) {
		return;
	}
	SIRVulkanDevice& device = getDevice<SIRVulkanDevice>();
	if ( m_Native.nLastGpuUse != 0 ) {
		const sirVulkanCompletion_t completion{ m_Native.nDeviceSerial, m_Native.nLastGpuUse };
		if ( device.waitCompletion( &completion, UINT64_MAX ) != SIRVulkanResult::SIR_VULKAN_SUCCESS ) {
			return;
		}
	}
	static_cast<void>( device.destroyComputePipeline( &m_Native ) );
}

#include "Renderer/Drivers/Vulkan/SIRVulkanDevice.h"
#include <Renderer/Drivers/Vulkan/SIRVulkanPipeline.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanShader.h>
#include <vulkan/vulkan_core.h>

// Vulkan pipeline creation is deliberately owned by the native device. Shader
// module and layout failure must leave the caller's pipeline slot untouched.
static VkFormat sirVulkanRasterFormat( SIRRenderFormat eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA8_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA16_FLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R16_FLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R32_FLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RG32_FLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_RGBA32_FLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case SIRRenderFormat::SIR_RENDER_FORMAT_D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIRRenderFormat::SIR_RENDER_FORMAT_D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}

static VkPipeline createPipeline( const SIRVulkanDevice& device, const sirRenderPipelineDesc_t& desc )
{
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = desc.nColorFormatCount;
	renderingInfo.depthAttachmentFormat = sirVulkanRasterFormat( desc.eDepthFormat );
	renderingInfo.stencilAttachmentFormat = sirVulkanRasterFormat();

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	VkPipeline hPipeline{};
	VkResult eResult = device.createGraphicsPipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos, const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines)
}

SIRVulkanPipeline::SIRVulkanPipeline( const SIRSharedPtr<SIRRenderDevice>& device,
                                      const SIRSharedPtr<SIRVulkanBindingLayout>& layout,
                                      const SIRSharedPtr<SIRRenderShader>& shader,
                                      const sirContainerAllocator_t *pAllocator ) noexcept
    : SIRRenderPipeline( device, SIRRenderPipelineType::SIR_RENDER_COMPUTE, layout, pAllocator )
{
	if ( !shader.isValid() ) {
		return;
	}

	m_Shaders.tryEmplaceBack( shader );
	const SIRVulkanDevice& vkDevice = getDevice<SIRVulkanDevice>();
	const SIRVulkanShader *pShader = dynamic_cast<const SIRVulkanShader *>( shader.get() );
	const VkDescriptorSetLayout hLayout = layout->isValid() ? layout->getNativeLayout() : VK_NULL_HANDLE;

	if ( shader.isValid() ) {
		m_Shaders.tryEmplaceBack( shader );
		SIRVulkanDevice *pDevice = dynamic_cast<SIRVulkanDevice *>( device.get() );
		const SIRVulkanShader *pShader = dynamic_cast<const SIRVulkanShader *>( shader.get() );
		const VkDescriptorSetLayout hSetLayout = layout.isValid() ? layout->getNativeLayout() : VK_NULL_HANDLE;

		if ( pShader->isValid() && pShader->getStage() == static_cast<uint32_t>( SIRRenderShaderStage::SIR_RENDER_COMPUTE ) &&
		     ( !layout.isValid() || hSetLayout != VK_NULL_HANDLE ) ) {
			const sirRenderShaderDescription_t& description = shader->getDescription();
			pDevice->createComputePipeline(
			    pShader->getNativeModule(), description.pszEntryPoint, layout.isValid() ? &hSetLayout : nullptr, layout.isValid() ? 1U : 0U, &m_Native );
		}
	}
}

SIRVulkanPipeline::SIRVulkanPipeline( const SIRSharedPtr<SIRRenderDevice>& device,
                                      const SIRSharedPtr<SIRVulkanBindingLayout>& layout,
                                      const SIRSharedPtr<SIRRenderShader>& vertexShader,
                                      const SIRSharedPtr<SIRRenderShader>& fragmentShader,
                                      const sirGraphicsPipelineDesc_t& description,
                                      const sirContainerAllocator_t *pAllocator ) noexcept
    : SIRRenderPipeline( device, SIRRenderPipelineType::SIR_RENDER_GRAPHICS, layout, pAllocator )
{
	if ( vertexShader.isValid() )
		m_Shaders.tryEmplaceBack( vertexShader );
	if ( fragmentShader.isValid() )
		m_Shaders.tryEmplaceBack( fragmentShader );
	SIRVulkanDevice *pDevice = dynamic_cast<SIRVulkanDevice *>( device.get() );
	const SIRVulkanShader *pVertex = dynamic_cast<const SIRVulkanShader *>( vertexShader.get() );
	const SIRVulkanShader *pFragment = dynamic_cast<const SIRVulkanShader *>( fragmentShader.get() );
	const VkDescriptorSetLayout hSetLayout = layout.isValid() ? layout->getNativeLayout() : VK_NULL_HANDLE;
	if ( pDevice != nullptr && pVertex != nullptr && pFragment != nullptr && pVertex->isValid() && pFragment->isValid() &&
	     pVertex->getStage() == static_cast<uint32_t>( SIRRenderShaderStage::SIR_RENDER_VERTEX ) &&
	     pFragment->getStage() == static_cast<uint32_t>( SIRRenderShaderStage::SIR_RENDER_FRAGMENT ) && ( !layout.isValid() || hSetLayout != VK_NULL_HANDLE ) ) {
		sirGraphicsPipelineDesc_t nativeDescription = description;
		// Binding metadata is owned by SIRVulkanBindingLayout. The legacy
		// descriptor array is deliberately not consulted by native creation.
		nativeDescription.pBindings = nullptr;
		nativeDescription.nBindingCount = 0;
		pDevice->createRasterPipeline(
		    nativeDescription, pVertex->getNativeModule(), pFragment->getNativeModule(), layout.isValid() ? &hSetLayout : nullptr, layout.isValid() ? 1U : 0U, &m_Native );
	}
}

SIRVulkanPipeline::~SIRVulkanPipeline() noexcept
{
	SIRVulkanDevice *pDevice = dynamic_cast<SIRVulkanDevice *>( getDevice().get() );
	if ( pDevice != nullptr && m_Native.hPipeline != VK_NULL_HANDLE ) {
		pDevice->waitIdle();
		pDevice->destroyComputePipeline( &m_Native );
	}
}

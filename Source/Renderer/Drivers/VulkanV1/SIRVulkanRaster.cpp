/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>
#include <Renderer/Drivers/Vulkan/SIRVulkanProgram.h>

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

static VkPrimitiveTopology sirVulkanRasterTopology( SIRRenderTopology eTopology ) noexcept
{
	switch ( eTopology ) {
	case SIRRenderTopology::SIR_RENDER_TOPOLOGY_POINTS:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case SIRRenderTopology::SIR_RENDER_TOPOLOGY_LINES:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case SIRRenderTopology::SIR_RENDER_TOPOLOGY_LINE_STRIP:
		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case SIRRenderTopology::SIR_RENDER_TOPOLOGY_TRIANGLES:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case SIRRenderTopology::SIR_RENDER_TOPOLOGY_TRIANGLE_STRIP:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	default:
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}
}

static VkCompareOp sirVulkanRasterCompare( SIRRenderCompareOp eCompare ) noexcept
{
	static constexpr VkCompareOp OPS[] = { VK_COMPARE_OP_NEVER,
	                                       VK_COMPARE_OP_LESS,
	                                       VK_COMPARE_OP_EQUAL,
	                                       VK_COMPARE_OP_LESS_OR_EQUAL,
	                                       VK_COMPARE_OP_GREATER,
	                                       VK_COMPARE_OP_NOT_EQUAL,
	                                       VK_COMPARE_OP_GREATER_OR_EQUAL,
	                                       VK_COMPARE_OP_ALWAYS };
	const uint32_t iCompare = static_cast<uint32_t>( eCompare );
	return iCompare <= static_cast<uint32_t>( SIRRenderCompareOp::SIR_RENDER_COMPARE_ALWAYS ) ? OPS[iCompare] : VK_COMPARE_OP_MAX_ENUM;
}

static VkBlendFactor sirVulkanRasterBlendFactor( SIRRenderBlendFactor eFactor ) noexcept
{
	static constexpr VkBlendFactor FACTORS[] = { VK_BLEND_FACTOR_ZERO,
	                                             VK_BLEND_FACTOR_ONE,
	                                             VK_BLEND_FACTOR_SRC_COLOR,
	                                             VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	                                             VK_BLEND_FACTOR_DST_COLOR,
	                                             VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	                                             VK_BLEND_FACTOR_SRC_ALPHA,
	                                             VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	                                             VK_BLEND_FACTOR_DST_ALPHA,
	                                             VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };
	const uint32_t iFactor = static_cast<uint32_t>( eFactor );
	return iFactor <= static_cast<uint32_t>( SIRRenderBlendFactor::SIR_RENDER_BLEND_ONE_MINUS_DST_ALPHA ) ? FACTORS[iFactor] : VK_BLEND_FACTOR_MAX_ENUM;
}

static VkBlendOp sirVulkanRasterBlendOp( SIRRenderBlendOp eOperation ) noexcept
{
	static constexpr VkBlendOp OPS[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN, VK_BLEND_OP_MAX };
	const uint32_t iOperation = static_cast<uint32_t>( eOperation );
	return iOperation <= static_cast<uint32_t>( SIRRenderBlendOp::SIR_RENDER_BLEND_MAX ) ? OPS[iOperation] : VK_BLEND_OP_MAX_ENUM;
}

SIRVulkanResult SIRVulkanDevice::createRasterPipeline( const SIRRenderPipelineDesc& desc, const SIRVulkanProgram& program,
                                                       VkDescriptorSetLayout hSetLayout, sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	const SIRVulkanShader *pVertex = program.getShader( SIRRenderShaderStage::SIR_RENDER_STAGE_VERTEX );
	const SIRVulkanShader *pFragment = program.getShader( SIRRenderShaderStage::SIR_RENDER_STAGE_FRAGMENT );
	if ( !m_bReady || m_bDeviceLost ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline != VK_NULL_HANDLE || pPipeline->hLayout != VK_NULL_HANDLE ||
	     pVertex == nullptr || pFragment == nullptr || !pVertex->isValid() || !pFragment->isValid() ||
	     desc.eType != SIRRenderPipelineType::SIR_RENDER_PIPELINE_GRAPHICS || desc.vertexAttributes.getSize() > 32 ||
	     desc.colorFormats.getSize() > 8 || ( desc.colorFormats.getSize() == 0 && desc.eDepthFormat == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ) ||
	     desc.eTopology == SIRRenderTopology::SIR_RENDER_TOPOLOGY_PATCHES || desc.nSampleCount != 1 ||
	     desc.blendStates.getSize() < desc.colorFormats.getSize() ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	const PFN_vkCreatePipelineLayout pfnCreateLayout = m_Procs.vkCreatePipelineLayout;
	const PFN_vkDestroyPipelineLayout pfnDestroyLayout = m_Procs.vkDestroyPipelineLayout;
	const PFN_vkCreateGraphicsPipelines pfnCreateGraphics = m_Procs.vkCreateGraphicsPipelines;
	const PFN_vkDestroyPipeline pfnDestroyPipeline = m_Procs.vkDestroyPipeline;
	if ( pfnCreateLayout == nullptr || pfnDestroyLayout == nullptr || pfnCreateGraphics == nullptr || pfnDestroyPipeline == nullptr ) {
		return SIRVulkanResult::SIR_VULKAN_API_UNSUPPORTED;
	}
	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkVertexInputAttributeDescription attributes[32];
	memset( attributes, 0, sizeof( attributes ) );
	VkVertexInputBindingDescription bindings[32];
	memset( bindings, 0, sizeof( bindings ) );
	uint32_t nBindings = 0;
	for ( uint32_t i = 0; i < desc.vertexAttributes.getSize(); ++i ) {
		const SIRRenderVertexAttribute& attribute = desc.vertexAttributes[i];
		const VkFormat eFormat = sirVulkanRasterFormat( attribute.eFormat );
		if ( eFormat == VK_FORMAT_UNDEFINED || attribute.nStrideBytes == 0 || attribute.nBufferSlot >= 32 || attribute.nLocation >= 32 ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( desc.vertexAttributes[j].nLocation == attribute.nLocation ) {
				return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
			}
		}
		attributes[i] = { attribute.nLocation, attribute.nBufferSlot, eFormat, attribute.nOffsetBytes };
		uint32_t iBinding = 0;
		while ( iBinding < nBindings && bindings[iBinding].binding != attribute.nBufferSlot ) {
			++iBinding;
		}
		if ( iBinding == nBindings ) {
			bindings[nBindings++] = { attribute.nBufferSlot, attribute.nStrideBytes, VK_VERTEX_INPUT_RATE_VERTEX };
		} else if ( bindings[iBinding].stride != attribute.nStrideBytes ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
	}
	vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>( desc.vertexAttributes.getSize() );
	vertexInput.pVertexAttributeDescriptions = attributes;
	vertexInput.vertexBindingDescriptionCount = nBindings;
	vertexInput.pVertexBindingDescriptions = bindings;

	VkFormat colorFormats[8];
	memset( colorFormats, 0, sizeof( colorFormats ) );
	VkPipelineColorBlendAttachmentState blendStates[8];
	memset( blendStates, 0, sizeof( blendStates ) );
	for ( uint32_t i = 0; i < desc.colorFormats.getSize(); ++i ) {
		colorFormats[i] = sirVulkanRasterFormat( desc.colorFormats[i] );
		if ( colorFormats[i] == VK_FORMAT_UNDEFINED || ( sirVulkanImageAspectMask( colorFormats[i] ) & VK_IMAGE_ASPECT_COLOR_BIT ) == 0 ) {
			return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
		}
		const SIRRenderBlendState& blend = desc.blendStates[i];
		VkPipelineColorBlendAttachmentState& native = blendStates[i];
		native.blendEnable = blend.bEnabled ? VK_TRUE : VK_FALSE;
		native.srcColorBlendFactor = sirVulkanRasterBlendFactor( blend.eSourceColor );
		native.dstColorBlendFactor = sirVulkanRasterBlendFactor( blend.eDestinationColor );
		native.colorBlendOp = sirVulkanRasterBlendOp( blend.eColorOperation );
		native.srcAlphaBlendFactor = sirVulkanRasterBlendFactor( blend.eSourceAlpha );
		native.dstAlphaBlendFactor = sirVulkanRasterBlendFactor( blend.eDestinationAlpha );
		native.alphaBlendOp = sirVulkanRasterBlendOp( blend.eAlphaOperation );
		native.colorWriteMask = blend.nColorWriteMask == 0 && !blend.bEnabled ? 0xF : blend.nColorWriteMask;
	}
	const VkFormat eDepth = sirVulkanRasterFormat( desc.eDepthFormat );
	if ( desc.eDepthFormat != SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN &&
	     ( eDepth == VK_FORMAT_UNDEFINED || ( sirVulkanImageAspectMask( eDepth ) & VK_IMAGE_ASPECT_DEPTH_BIT ) == 0 ) ) {
		return SIRVulkanResult::SIR_VULKAN_INVALID_ARGUMENT;
	}
	VkPipelineLayout hLayout = VK_NULL_HANDLE;
	const uint32_t nSetLayoutCount = hSetLayout == VK_NULL_HANDLE ? 0U : 1U;
	VkPipeline hPipeline = VK_NULL_HANDLE;
	VkResult eCreate = VK_SUCCESS;
	if ( eCreate == VK_SUCCESS ) {
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = nSetLayoutCount;
		layoutInfo.pSetLayouts = nSetLayoutCount == 0 ? nullptr : &hSetLayout;
		eCreate = pfnCreateLayout( m_Device, &layoutInfo, nullptr, &hLayout );
	}
	if ( eCreate == VK_SUCCESS ) {
		VkPipelineShaderStageCreateInfo stages[2];
		memset( stages, 0, sizeof( stages ) );
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = pVertex->getNativeModule();
		stages[0].pName = pVertex->getEntryPoint();
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = pFragment->getNativeModule();
		stages[1].pName = pFragment->getEntryPoint();
		VkPipelineInputAssemblyStateCreateInfo assembly{};
		assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly.topology = sirVulkanRasterTopology( desc.eTopology );
		VkPipelineViewportStateCreateInfo viewport{};
		viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport.viewportCount = 1;
		viewport.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo raster{};
		raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster.polygonMode = VK_POLYGON_MODE_FILL;
		raster.lineWidth = 1.0f;
		raster.cullMode = desc.eCullMode == SIRRenderCullMode::SIR_RENDER_CULL_NONE    ? VK_CULL_MODE_NONE
		                  : desc.eCullMode == SIRRenderCullMode::SIR_RENDER_CULL_FRONT ? VK_CULL_MODE_FRONT_BIT
		                                                                                   : VK_CULL_MODE_BACK_BIT;
		raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VkPipelineMultisampleStateCreateInfo multisample{};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineDepthStencilStateCreateInfo depth{};
		depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth.depthTestEnable = desc.bDepthTest ? VK_TRUE : VK_FALSE;
		depth.depthWriteEnable = desc.bDepthWrite ? VK_TRUE : VK_FALSE;
		depth.depthCompareOp = sirVulkanRasterCompare( desc.eDepthCompare );
		VkPipelineColorBlendStateCreateInfo blend{};
		blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend.attachmentCount = static_cast<uint32_t>( desc.colorFormats.getSize() );
		blend.pAttachments = blendStates;
		VkDynamicState states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamic{};
		dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = states;
		VkPipelineRenderingCreateInfo rendering{};
		rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		rendering.colorAttachmentCount = static_cast<uint32_t>( desc.colorFormats.getSize() );
		rendering.pColorAttachmentFormats = colorFormats;
		rendering.depthAttachmentFormat = eDepth;
		rendering.stencilAttachmentFormat = eDepth == VK_FORMAT_D24_UNORM_S8_UINT ? eDepth : VK_FORMAT_UNDEFINED;
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &rendering;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &assembly;
		pipelineInfo.pViewportState = &viewport;
		pipelineInfo.pRasterizationState = &raster;
		pipelineInfo.pMultisampleState = &multisample;
		pipelineInfo.pDepthStencilState = &depth;
		pipelineInfo.pColorBlendState = &blend;
		pipelineInfo.pDynamicState = &dynamic;
		pipelineInfo.layout = hLayout;
		eCreate = pfnCreateGraphics( m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hPipeline );
	}
	if ( eCreate != VK_SUCCESS || hPipeline == VK_NULL_HANDLE ) {
		if ( hPipeline != VK_NULL_HANDLE )
			pfnDestroyPipeline( m_Device, hPipeline, nullptr );
		if ( hLayout != VK_NULL_HANDLE )
			pfnDestroyLayout( m_Device, hLayout, nullptr );
		m_bDeviceLost = eCreate == VK_ERROR_DEVICE_LOST;
		return SIRVulkanResult::SIR_VULKAN_NATIVE_FAILURE;
	}
	pPipeline->hPipeline = hPipeline;
	pPipeline->hLayout = hLayout;
	pPipeline->nDeviceSerial = m_nDeviceSerial;
	pPipeline->nLastGpuUse = 0;
	pPipeline->bGraphics = true;
	pPipeline->nColorFormatCount = static_cast<uint32_t>( desc.colorFormats.getSize() );
	for ( uint32_t i = 0; i < desc.colorFormats.getSize(); ++i ) {
		pPipeline->szColorFormats[i] = colorFormats[i];
	}
	pPipeline->eDepthFormat = eDepth;
	pPipeline->nSetLayoutCount = nSetLayoutCount;
	pPipeline->bOwnsSetLayouts = false;
	for ( uint32_t i = 0; i < nSetLayoutCount; ++i )
		pPipeline->szSetLayouts[i] = hSetLayout;
	++m_nLivePipelines;
	return SIRVulkanResult::SIR_VULKAN_SUCCESS;
}

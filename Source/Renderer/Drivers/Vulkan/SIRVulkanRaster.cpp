/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/Vulkan/SIRVulkanDevice.h>
#include <Renderer/SIRGraphicsTypes.h>

// Vulkan pipeline creation is deliberately owned by the native device. Shader
// module and layout failure must leave the caller's pipeline slot untouched.
static VkFormat sirVulkanRasterFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return VK_FORMAT_B8G8R8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return VK_FORMAT_R16_SFLOAT;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return VK_FORMAT_D24_UNORM_S8_UINT;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return VK_FORMAT_D32_SFLOAT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}

VkDescriptorType sirVulkanDescriptorType( sirGraphicsBindingType_t eType ) noexcept
{
	switch ( eType ) {
	case SIR_GRAPHICS_BINDING_CONSTANT_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case SIR_GRAPHICS_BINDING_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case SIR_GRAPHICS_BINDING_STORAGE_TEXTURE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case SIR_GRAPHICS_BINDING_SAMPLER:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	default:
		return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

static VkPrimitiveTopology sirVulkanRasterTopology( sirGraphicsTopology_t eTopology ) noexcept
{
	switch ( eTopology ) {
	case SIR_GRAPHICS_TOPOLOGY_POINTS:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	case SIR_GRAPHICS_TOPOLOGY_LINES:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLES:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	default:
		return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
	}
}

static VkCompareOp sirVulkanRasterCompare( sirGraphicsCompareOp_t eCompare ) noexcept
{
	static constexpr VkCompareOp OPS[] = { VK_COMPARE_OP_NEVER,
	                                       VK_COMPARE_OP_LESS,
	                                       VK_COMPARE_OP_EQUAL,
	                                       VK_COMPARE_OP_LESS_OR_EQUAL,
	                                       VK_COMPARE_OP_GREATER,
	                                       VK_COMPARE_OP_NOT_EQUAL,
	                                       VK_COMPARE_OP_GREATER_OR_EQUAL,
	                                       VK_COMPARE_OP_ALWAYS };
	return eCompare <= SIR_GRAPHICS_COMPARE_ALWAYS ? OPS[eCompare] : VK_COMPARE_OP_MAX_ENUM;
}

static VkBlendFactor sirVulkanRasterBlendFactor( sirGraphicsBlendFactor_t eFactor ) noexcept
{
	static constexpr VkBlendFactor FACTORS[] = { VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
	                                             VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	                                             VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	                                             VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	                                             VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };
	return eFactor <= SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ? FACTORS[eFactor] : VK_BLEND_FACTOR_MAX_ENUM;
}

static VkBlendOp sirVulkanRasterBlendOp( sirGraphicsBlendOp_t eOperation ) noexcept
{
	static constexpr VkBlendOp OPS[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN,
	                                     VK_BLEND_OP_MAX };
	return eOperation <= SIR_GRAPHICS_BLEND_MAX ? OPS[eOperation] : VK_BLEND_OP_MAX_ENUM;
}

sirVulkanResult_t SIRVulkanDevice::createRasterPipeline( const sirGraphicsPipelineDesc_t& desc,
                                                         sirVulkanComputePipeline_t *pPipeline ) noexcept
{
	if ( !m_bReady || m_bDeviceLost ) {
		return SIR_VULKAN_INVALID_STATE;
	}
	if ( pPipeline == nullptr || pPipeline->hPipeline != VK_NULL_HANDLE || pPipeline->hLayout != VK_NULL_HANDLE ||
	     desc.eType != SIR_GRAPHICS_PIPELINE_GRAPHICS || desc.nBindingCount > 32 || desc.nAttributeCount > 32 ||
	     desc.nColorFormatCount > 8 || desc.nColorFormatCount == 0 && desc.eDepthFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     desc.eTopology == SIR_GRAPHICS_TOPOLOGY_PATCHES || desc.nSampleCount != 1 ||
	     !sirVulkanValidateSpirv( desc.vertexShader.pBytes, desc.vertexShader.nByteCount, desc.vertexShader.pszEntryPoint ) ||
	     !sirVulkanValidateSpirv( desc.pixelShader.pBytes, desc.pixelShader.nByteCount, desc.pixelShader.pszEntryPoint ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	uint32_t nSetLayoutCount = 0;
	for ( uint32_t i = 0; i < desc.nBindingCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = desc.pBindings[i];
		if ( binding.nSpace >= 4 || binding.nBinding >= 64 || binding.nArrayCount == 0 || binding.nArrayCount > 1024 ||
		     binding.nShaderStageMask == 0 ||
		     ( binding.nShaderStageMask & ~( SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT ) ) != 0 ||
		     sirVulkanDescriptorType( binding.eType ) == VK_DESCRIPTOR_TYPE_MAX_ENUM )
			return SIR_VULKAN_INVALID_ARGUMENT;
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( desc.pBindings[j].nSpace == binding.nSpace && desc.pBindings[j].nBinding == binding.nBinding )
				return SIR_VULKAN_INVALID_ARGUMENT;
		}
		if ( nSetLayoutCount <= binding.nSpace )
			nSetLayoutCount = binding.nSpace + 1;
	}
	const PFN_vkCreateShaderModule pfnCreateShader = ( PFN_vkCreateShaderModule )m_pfnGetDeviceProcAddr( m_Device, "vkCreateShaderModule" );
	const PFN_vkDestroyShaderModule pfnDestroyShader =
	    ( PFN_vkDestroyShaderModule )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyShaderModule" );
	const PFN_vkCreatePipelineLayout pfnCreateLayout =
	    ( PFN_vkCreatePipelineLayout )m_pfnGetDeviceProcAddr( m_Device, "vkCreatePipelineLayout" );
	const PFN_vkDestroyPipelineLayout pfnDestroyLayout =
	    ( PFN_vkDestroyPipelineLayout )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipelineLayout" );
	const PFN_vkCreateGraphicsPipelines pfnCreateGraphics =
	    ( PFN_vkCreateGraphicsPipelines )m_pfnGetDeviceProcAddr( m_Device, "vkCreateGraphicsPipelines" );
	const PFN_vkDestroyPipeline pfnDestroyPipeline = ( PFN_vkDestroyPipeline )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyPipeline" );
	const PFN_vkCreateDescriptorSetLayout pfnCreateSetLayout =
	    ( PFN_vkCreateDescriptorSetLayout )m_pfnGetDeviceProcAddr( m_Device, "vkCreateDescriptorSetLayout" );
	const PFN_vkDestroyDescriptorSetLayout pfnDestroySetLayout =
	    ( PFN_vkDestroyDescriptorSetLayout )m_pfnGetDeviceProcAddr( m_Device, "vkDestroyDescriptorSetLayout" );
	if ( pfnCreateShader == nullptr || pfnDestroyShader == nullptr || pfnCreateLayout == nullptr || pfnDestroyLayout == nullptr ||
	     pfnCreateGraphics == nullptr || pfnDestroyPipeline == nullptr || pfnCreateSetLayout == nullptr ||
	     pfnDestroySetLayout == nullptr ) {
		return SIR_VULKAN_API_UNSUPPORTED;
	}
	VkPipelineVertexInputStateCreateInfo vertexInput = {};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VkVertexInputAttributeDescription attributes[32] = {};
	VkVertexInputBindingDescription bindings[32] = {};
	uint32_t nBindings = 0;
	for ( uint32_t i = 0; i < desc.nAttributeCount; ++i ) {
		const sirGraphicsVertexAttribute_t& attribute = desc.pAttributes[i];
		const VkFormat eFormat = sirVulkanRasterFormat( attribute.eFormat );
		if ( eFormat == VK_FORMAT_UNDEFINED || attribute.nStrideBytes == 0 || attribute.nBufferSlot >= 32 || attribute.nLocation >= 32 ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( desc.pAttributes[j].nLocation == attribute.nLocation ) {
				return SIR_VULKAN_INVALID_ARGUMENT;
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
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
	}
	vertexInput.vertexAttributeDescriptionCount = desc.nAttributeCount;
	vertexInput.pVertexAttributeDescriptions = attributes;
	vertexInput.vertexBindingDescriptionCount = nBindings;
	vertexInput.pVertexBindingDescriptions = bindings;

	VkFormat colorFormats[8] = {};
	VkPipelineColorBlendAttachmentState blendStates[8] = {};
	for ( uint32_t i = 0; i < desc.nColorFormatCount; ++i ) {
		colorFormats[i] = sirVulkanRasterFormat( desc.szColorFormats[i] );
		if ( colorFormats[i] == VK_FORMAT_UNDEFINED || ( sirVulkanImageAspectMask( colorFormats[i] ) & VK_IMAGE_ASPECT_COLOR_BIT ) == 0 ) {
			return SIR_VULKAN_INVALID_ARGUMENT;
		}
		const sirGraphicsBlendState_t& blend = desc.szBlendStates[i];
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
	if ( desc.eDepthFormat != SIR_GRAPHICS_FORMAT_UNKNOWN &&
	     ( eDepth == VK_FORMAT_UNDEFINED || ( sirVulkanImageAspectMask( eDepth ) & VK_IMAGE_ASPECT_DEPTH_BIT ) == 0 ) ) {
		return SIR_VULKAN_INVALID_ARGUMENT;
	}
	VkShaderModule hVertex = VK_NULL_HANDLE;
	VkShaderModule hFragment = VK_NULL_HANDLE;
	VkPipelineLayout hLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout hSetLayouts[4] = {};
	VkPipeline hPipeline = VK_NULL_HANDLE;
	VkShaderModuleCreateInfo shaderInfo = {};
	shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderInfo.codeSize = ( size_t )desc.vertexShader.nByteCount;
	shaderInfo.pCode = ( const uint32_t * )desc.vertexShader.pBytes;
	VkResult eCreate = pfnCreateShader( m_Device, &shaderInfo, nullptr, &hVertex );
	if ( eCreate == VK_SUCCESS ) {
		shaderInfo.codeSize = ( size_t )desc.pixelShader.nByteCount;
		shaderInfo.pCode = ( const uint32_t * )desc.pixelShader.pBytes;
		eCreate = pfnCreateShader( m_Device, &shaderInfo, nullptr, &hFragment );
	}
	if ( eCreate == VK_SUCCESS ) {
		for ( uint32_t iSet = 0; iSet < nSetLayoutCount; ++iSet ) {
			VkDescriptorSetLayoutBinding bindings[32] = {};
			uint32_t nBindings = 0;
			for ( uint32_t i = 0; i < desc.nBindingCount; ++i ) {
				const sirGraphicsBindingDesc_t& binding = desc.pBindings[i];
				if ( binding.nSpace != iSet )
					continue;
				VkDescriptorSetLayoutBinding& native = bindings[nBindings++];
				native.binding = binding.nBinding;
				native.descriptorType = sirVulkanDescriptorType( binding.eType );
				native.descriptorCount = binding.nArrayCount;
				native.stageFlags = ( ( binding.nShaderStageMask & SIR_GRAPHICS_STAGE_VERTEX ) != 0 ? VK_SHADER_STAGE_VERTEX_BIT : 0 ) |
				                    ( ( binding.nShaderStageMask & SIR_GRAPHICS_STAGE_FRAGMENT ) != 0 ? VK_SHADER_STAGE_FRAGMENT_BIT : 0 );
			}
			VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
			setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			setLayoutInfo.bindingCount = nBindings;
			setLayoutInfo.pBindings = bindings;
			eCreate = pfnCreateSetLayout( m_Device, &setLayoutInfo, nullptr, &hSetLayouts[iSet] );
			if ( eCreate != VK_SUCCESS )
				break;
		}
	}
	if ( eCreate == VK_SUCCESS ) {
		VkPipelineLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = nSetLayoutCount;
		layoutInfo.pSetLayouts = hSetLayouts;
		eCreate = pfnCreateLayout( m_Device, &layoutInfo, nullptr, &hLayout );
	}
	if ( eCreate == VK_SUCCESS ) {
		VkPipelineShaderStageCreateInfo stages[2] = {};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = hVertex;
		stages[0].pName = desc.vertexShader.pszEntryPoint;
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = hFragment;
		stages[1].pName = desc.pixelShader.pszEntryPoint;
		VkPipelineInputAssemblyStateCreateInfo assembly = {};
		assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		assembly.topology = sirVulkanRasterTopology( desc.eTopology );
		VkPipelineViewportStateCreateInfo viewport = {};
		viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport.viewportCount = 1;
		viewport.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo raster = {};
		raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster.polygonMode = VK_POLYGON_MODE_FILL;
		raster.lineWidth = 1.0f;
		raster.cullMode = desc.eCullMode == SIR_GRAPHICS_CULL_NONE    ? VK_CULL_MODE_NONE
		                  : desc.eCullMode == SIR_GRAPHICS_CULL_FRONT ? VK_CULL_MODE_FRONT_BIT
		                                                              : VK_CULL_MODE_BACK_BIT;
		raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		VkPipelineMultisampleStateCreateInfo multisample = {};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineDepthStencilStateCreateInfo depth = {};
		depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth.depthTestEnable = desc.bDepthTest ? VK_TRUE : VK_FALSE;
		depth.depthWriteEnable = desc.bDepthWrite ? VK_TRUE : VK_FALSE;
		depth.depthCompareOp = sirVulkanRasterCompare( desc.eDepthCompare );
		VkPipelineColorBlendStateCreateInfo blend = {};
		blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend.attachmentCount = desc.nColorFormatCount;
		blend.pAttachments = blendStates;
		VkDynamicState states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamic = {};
		dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic.dynamicStateCount = 2;
		dynamic.pDynamicStates = states;
		VkPipelineRenderingCreateInfo rendering = {};
		rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		rendering.colorAttachmentCount = desc.nColorFormatCount;
		rendering.pColorAttachmentFormats = colorFormats;
		rendering.depthAttachmentFormat = eDepth;
		rendering.stencilAttachmentFormat = eDepth == VK_FORMAT_D24_UNORM_S8_UINT ? eDepth : VK_FORMAT_UNDEFINED;
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
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
	if ( hVertex != VK_NULL_HANDLE )
		pfnDestroyShader( m_Device, hVertex, nullptr );
	if ( hFragment != VK_NULL_HANDLE )
		pfnDestroyShader( m_Device, hFragment, nullptr );
	if ( eCreate != VK_SUCCESS || hPipeline == VK_NULL_HANDLE ) {
		if ( hPipeline != VK_NULL_HANDLE )
			pfnDestroyPipeline( m_Device, hPipeline, nullptr );
		if ( hLayout != VK_NULL_HANDLE )
			pfnDestroyLayout( m_Device, hLayout, nullptr );
		for ( uint32_t i = 0; i < nSetLayoutCount; ++i ) {
			if ( hSetLayouts[i] != VK_NULL_HANDLE )
				pfnDestroySetLayout( m_Device, hSetLayouts[i], nullptr );
		}
		m_bDeviceLost = eCreate == VK_ERROR_DEVICE_LOST;
		return SIR_VULKAN_NATIVE_FAILURE;
	}
	pPipeline->hPipeline = hPipeline;
	pPipeline->hLayout = hLayout;
	pPipeline->nDeviceSerial = m_nDeviceSerial;
	pPipeline->nLastGpuUse = 0;
	pPipeline->bGraphics = true;
	pPipeline->nColorFormatCount = desc.nColorFormatCount;
	for ( uint32_t i = 0; i < desc.nColorFormatCount; ++i ) {
		pPipeline->szColorFormats[i] = colorFormats[i];
	}
	pPipeline->eDepthFormat = eDepth;
	pPipeline->nSetLayoutCount = nSetLayoutCount;
	pPipeline->nBindingCount = desc.nBindingCount;
	for ( uint32_t i = 0; i < desc.nBindingCount; ++i )
		pPipeline->szBindings[i] = desc.pBindings[i];
	for ( uint32_t i = 0; i < nSetLayoutCount; ++i )
		pPipeline->szSetLayouts[i] = hSetLayouts[i];
	++m_nLivePipelines;
	return SIR_VULKAN_SUCCESS;
}

#include "VulkanPipelineBuilder.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanPipelineBuilder::VulkanPipelineBuilder( const SVulkanInstance& instance, VkRenderPass hRenderPass, VulkanSwapChain *pSwapChain )
	: m_Instance( instance ), m_hRenderPass( hRenderPass ), m_pSwapChain( pSwapChain )
{
	VkPipelineCacheCreateInfo createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	vkCreatePipelineCache( m_Instance.hDevice, &createInfo, NULL, &m_hPipelineCache );
}

VulkanPipelineBuilder::~VulkanPipelineBuilder()
{
	for ( auto& it : m_DataCache ) {
		free( it.second );
	}
	if ( m_hPipelineCache != VK_NULL_HANDLE ) {
		vkDestroyPipelineCache( m_Instance.hDevice, m_hPipelineCache, NULL );
	}
}

void VulkanPipelineBuilder::StartPipeline( void )
{
	// TODO: use linear allocator
	m_pPipelineInfo = (SPipelineInfo *)calloc( sizeof( *m_pPipelineInfo ), 1 );
}

void VulkanPipelineBuilder::BindProgram( IRenderProgram *pProgram )
{
	if ( m_pPipelineInfo->arrShaderStages.size() > 0 ) {
		// warning
		return;
	}

	m_pPipelineInfo->arrShaderStages.reserve( ShaderType_Count );
	for ( uint32_t i = 0; i < ShaderType_Count; i++ ) {
		const SShaderModule *pModule = pProgram->GetModule( (EShaderType)i );

		if ( !pModule->pNativeHandle ) {
			// not created
			continue;
		}

		VkPipelineShaderStageCreateInfo stageInfo;
		memset( &stageInfo, 0, sizeof( stageInfo ) );
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.module = (VkShaderModule)pModule->pNativeHandle;
		stageInfo.pName = pModule->pszName;
		
		switch ( i ) {
			case ShaderType_Vertex:
				stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderType_Fragment:
				stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			case ShaderType_Geometry:
				stageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
				break;
			case ShaderType_Compute:
				stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
				break;
			case ShaderType_TesselationControl:
				stageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
				break;
			case ShaderType_TesselationEvaluation:
				stageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
				break;
		}
		m_pPipelineInfo->arrShaderStages.emplace_back( stageInfo );
	}
}

void VulkanPipelineBuilder::BindVertexLayout( IVertexLayout *pLayout )
{
	VulkanVertexLayout *pVulkanLayout = dynamic_cast<VulkanVertexLayout *>( pLayout );
	memcpy( &m_pPipelineInfo->VertexInputInfo, eastl::addressof( pVulkanLayout->GetVertexInfo() ), sizeof( m_pPipelineInfo->VertexInputInfo ) );
}

void VulkanPipelineBuilder::SetViewportState( const glm::vec2& size, const glm::vec2& position )
{
	m_pPipelineInfo->ViewportSize = size;
	m_pPipelineInfo->ViewportPosition = position;
}

void *VulkanPipelineBuilder::FinishPipeline( void )
{
	VkPipeline hPipeline;

	VkPipelineRasterizationStateCreateInfo rasterizer;
	memset( &rasterizer, 0, sizeof( rasterizer ) );
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling;
	memset( &multisampling, 0, sizeof( multisampling ) );
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	memset( &colorBlendAttachment, 0, sizeof( colorBlendAttachment ) );
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkViewport viewport;
	memset( &viewport, 0, sizeof( viewport ) );
	viewport.width = m_pPipelineInfo->ViewportSize.x;
	viewport.height = m_pPipelineInfo->ViewportSize.y;
	viewport.x = m_pPipelineInfo->ViewportPosition.x;
	viewport.y = m_pPipelineInfo->ViewportPosition.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent = m_pSwapChain->GetExtent();
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportState;
	memset( &viewportState, 0, sizeof( viewportState ) );
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pViewports = &viewport;
	viewportState.viewportCount = 1;
	viewportState.pScissors = &scissor;
	viewportState.scissorCount = 1;

	VkPipelineColorBlendStateCreateInfo colorBlending;
	memset( &colorBlending, 0, sizeof( colorBlending ) );
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[ 0 ] = 0.0f;
	colorBlending.blendConstants[ 1 ] = 0.0f;
	colorBlending.blendConstants[ 2 ] = 0.0f;
	colorBlending.blendConstants[ 3 ] = 0.0f;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	memset( &inputAssembly, 0, sizeof( inputAssembly ) );
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	memset( &depthStencilState, 0, sizeof( depthStencilState ) );
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	memset( &pipelineLayoutInfo, 0, sizeof( pipelineLayoutInfo ) );
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = NULL;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = NULL;

	const VkDynamicState arrDynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState;
	memset( &dynamicState, 0, sizeof( dynamicState ) );
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = SIREngine_ArrayLength( arrDynamicStates );
	dynamicState.pDynamicStates = arrDynamicStates;

	VkPipelineLayoutCreateInfo layoutInfo;
	memset( &layoutInfo, 0, sizeof( layoutInfo ) );
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkPipelineLayout hPipelineLayout;
	vkCreatePipelineLayout( m_Instance.hDevice, &layoutInfo, NULL, &hPipelineLayout );

	VkGraphicsPipelineCreateInfo graphicsCreateInfo;
	memset( &graphicsCreateInfo, 0, sizeof( graphicsCreateInfo ) );
	graphicsCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsCreateInfo.stageCount = m_pPipelineInfo->arrShaderStages.size();
	graphicsCreateInfo.pStages = m_pPipelineInfo->arrShaderStages.data();
	graphicsCreateInfo.pVertexInputState = &m_pPipelineInfo->VertexInputInfo;
	graphicsCreateInfo.pInputAssemblyState = &inputAssembly;
	graphicsCreateInfo.pViewportState = &viewportState;
	graphicsCreateInfo.pRasterizationState = &rasterizer;
	graphicsCreateInfo.pMultisampleState = &multisampling;
	graphicsCreateInfo.pColorBlendState = &colorBlending;
	graphicsCreateInfo.pDynamicState = &dynamicState;
	graphicsCreateInfo.layout = hPipelineLayout;
	graphicsCreateInfo.renderPass = m_hRenderPass;
	graphicsCreateInfo.subpass = 0;
	graphicsCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkResult result = vkCreateGraphicsPipelines( m_Instance.hDevice, m_hPipelineCache, 1, &graphicsCreateInfo, NULL, &hPipeline );

	m_DataCache.try_emplace( hPipeline, m_pPipelineInfo );

	return hPipeline;
}

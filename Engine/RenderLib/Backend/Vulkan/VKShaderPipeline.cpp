#include "VKShaderPipeline.h"
#include "VKShader.h"

extern CVar<uint32_t> e_MaxFPS;

static const VkFormat attribTypeVK[ NUMATTRIBTYPES ] = {
    VK_FORMAT_R32_SINT,
    VK_FORMAT_R32_SFLOAT,

    VK_FORMAT_R64_SINT,
    VK_FORMAT_R64_SFLOAT,

    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32B32A32_SFLOAT,
    
    VK_FORMAT_R16_SINT,
    VK_FORMAT_R16_UINT,
};

static const int attribTypeCountGL[ NUMATTRIBTYPES ] = {
    1,
    1,

    1,
    1,

    2,
    3,
    4,

    4,
    4,
};


SIRENGINE_FORCEINLINE uint64_t NumVertexAttribs( const VertexInputDescription_t& vertexInput )
{
    uint64_t i, numAttribs;

    numAttribs = 0;
    for ( i = 0; i < NumAttribs; i++ ) {
        const bool32 bEnabled = vertexInput.nEnabledVertexAttributes & SIRENGINE_BIT( i );

        if ( bEnabled ) {
            numAttribs++;
        }
    }

    return numAttribs;
}

VKShaderPipeline::VKShaderPipeline( void )
{
    VkPipelineCacheCreateInfo pipelineCacheInfo;
    memset( &pipelineCacheInfo, 0, sizeof( pipelineCacheInfo ) );
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if ( vkCreatePipelineCache( g_pVKContext->GetDevice(), &pipelineCacheInfo, NULL,
        &m_hVulkanDataCache ) != VK_SUCCESS )
    {

    }

    VkSamplerCreateInfo samplerBilinearInfo;
    memset( &samplerBilinearInfo, 0, sizeof( samplerBilinearInfo ) );
    samplerBilinearInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerBilinearInfo.minFilter = VK_FILTER_LINEAR;
    samplerBilinearInfo.magFilter = VK_FILTER_LINEAR;
    samplerBilinearInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerBilinearInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerBilinearInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerBilinearInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerBilinearInfo.minLod = 0.0f;
    samplerBilinearInfo.maxLod = 1.0f;
    samplerBilinearInfo.maxAnisotropy = 1.0f;
    samplerBilinearInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    if ( vkCreateSampler( g_pVKContext->GetDevice(), &samplerBilinearInfo, NULL, &m_Samplers[0] ) != VK_SUCCESS ) {

    }

    VkSamplerCreateInfo samplerNearestInfo;
    memset( &samplerNearestInfo, 0, sizeof( samplerNearestInfo ) );
    samplerNearestInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerNearestInfo.minFilter = VK_FILTER_NEAREST;
    samplerNearestInfo.magFilter = VK_FILTER_NEAREST;
    samplerNearestInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerNearestInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestInfo.minLod = 0.0f;
    samplerNearestInfo.maxLod = 1.0f;
    samplerNearestInfo.maxAnisotropy = 1.0f;
    samplerNearestInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSamplerCreateInfo samplerLinearNearestInfo;
    memset( &samplerLinearNearestInfo, 0, sizeof( samplerLinearNearestInfo ) );
    samplerLinearNearestInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerLinearNearestInfo.minFilter = VK_FILTER_LINEAR;
    samplerLinearNearestInfo.magFilter = VK_FILTER_NEAREST;
    samplerLinearNearestInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerLinearNearestInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerLinearNearestInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerLinearNearestInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerLinearNearestInfo.minLod = 0.0f;
    samplerLinearNearestInfo.maxLod = 1.0f;
    samplerLinearNearestInfo.maxAnisotropy = 1.0f;
    samplerLinearNearestInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSamplerCreateInfo samplerNearestLinearInfo;
    memset( &samplerNearestLinearInfo, 0, sizeof( samplerNearestLinearInfo ) );
    samplerNearestLinearInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerNearestLinearInfo.minFilter = VK_FILTER_NEAREST;
    samplerNearestLinearInfo.magFilter = VK_FILTER_LINEAR;
    samplerNearestLinearInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerNearestLinearInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestLinearInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestLinearInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerNearestLinearInfo.minLod = 0.0f;
    samplerNearestLinearInfo.maxLod = 1.0f;
    samplerNearestLinearInfo.maxAnisotropy = 1.0f;
    samplerNearestLinearInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
}

VKShaderPipeline::~VKShaderPipeline()
{
    ClearPipelineCache();

    if ( m_hVulkanDataCache ) {
        vkDestroyPipelineCache( g_pVKContext->GetDevice(), m_hVulkanDataCache, NULL );
    }
}

void VKShaderPipeline::ClearPipelineCache( void )
{
    for ( auto it : m_PipelineCache ) {
        if ( it->hDescriptorSetLayout ) {
            vkDestroyDescriptorSetLayout( g_pVKContext->GetDevice(), it->hDescriptorSetLayout, NULL );
        }
        if ( it->hLayout ) {
            vkDestroyPipelineLayout( g_pVKContext->GetDevice(), it->hLayout, NULL );
        }
        if ( it->hPipeline ) {
            vkDestroyPipeline( g_pVKContext->GetDevice(), it->hPipeline, NULL );
        }
    }
    m_PipelineCache.Clear();
}

uint64_t VKShaderPipeline::AddVertexAttribSet( const VertexInputDescription_t& vertexInput )
{
    uint64_t nCacheID;
    uint64_t nSize, nAttribCount, i, j;
    VKPipelineSet_t *pSet;

    nAttribCount = NumVertexAttribs( vertexInput );

    nSize = 0;
    nSize += SIRENGINE_PAD( sizeof( *pSet ), sizeof( uintptr_t ) );
    nSize += SIRENGINE_PAD( sizeof( *pSet->pAttributes ) * nAttribCount, sizeof( uintptr_t ) );
    nSize += SIRENGINE_PAD( sizeof( *pSet->pBindings ) * nAttribCount, sizeof( uintptr_t ) );

    pSet = (VKPipelineSet_t *)g_pVKContext->Alloc( nSize );
    pSet->pAttributes = (VkVertexInputAttributeDescription *)( pSet + 1 );
    pSet->pBindings = (VkVertexInputBindingDescription *)( pSet->pAttributes + nAttribCount );

    for ( i = 0; i < NumAttribs; i++ ) {
        const bool32 bEnabled = vertexInput.nEnabledVertexAttributes & SIRENGINE_BIT( i );

        if ( bEnabled ) {
            pSet->pBindings[i].binding = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pBindings[i].stride = szDefaultVertexAttribs[i].nStride;
            pSet->pBindings[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            pSet->pAttributes[i].binding = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pAttributes[i].location = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pAttributes[i].offset = szDefaultVertexAttribs[i].nOffset;
            pSet->pAttributes[i].format = attribTypeVK[ szDefaultVertexAttribs[i].nType ];
        }
    }

    {
        
    }
    
    {
        AllocateUniformBufferLayout( pSet );

        VkPipelineLayoutCreateInfo createInfo;
        memset( &createInfo, 0, sizeof( createInfo ) );
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pushConstantRangeCount = 0;
        createInfo.setLayoutCount = 1;
        createInfo.pSetLayouts = &pSet->hDescriptorSetLayout;

        if ( vkCreatePipelineLayout( g_pVKContext->GetDevice(), &createInfo, NULL,
            &m_PipelineCache.Last()->hLayout ) != VK_SUCCESS )
        {

        }
    }

    {
        const VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState;
        memset( &dynamicState, 0, sizeof( dynamicState ) );
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = SIREngine_StaticArrayLength( dynamicStates );
        dynamicState.pDynamicStates = dynamicStates;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo;
        memset( &vertexInputInfo, 0, sizeof( vertexInputInfo ) );
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = nAttribCount;
        vertexInputInfo.vertexAttributeDescriptionCount = nAttribCount;
        vertexInputInfo.pVertexAttributeDescriptions = pSet->pAttributes;
        vertexInputInfo.pVertexBindingDescriptions = pSet->pBindings;

        VkPipelineShaderStageCreateInfo fragmentStage;
        memset( &fragmentStage, 0, sizeof( fragmentStage ) );
        fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentStage.pName = vertexInput.pShader->GetName();
        fragmentStage.module = dynamic_cast<VKShader *>( vertexInput.pShader->GetPixelShader() )->GetVKModule();

        VkPipelineShaderStageCreateInfo vertexStage;
        memset( &vertexStage, 0, sizeof( vertexStage ) );
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.pName = vertexInput.pShader->GetName();
        vertexStage.module = dynamic_cast<VKShader *>( vertexInput.pShader->GetVertexShader() )->GetVKModule();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexStage,
            fragmentStage
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly;
        memset( &inputAssembly, 0, sizeof( inputAssembly ) );
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

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

        VkPipelineMultisampleStateCreateInfo multisampling;
        memset( &multisampling, 0, sizeof( multisampling ) );
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        memset( &colorBlendAttachment, 0, sizeof( colorBlendAttachment ) );
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending;
        memset( &colorBlending, 0, sizeof( colorBlending ) );
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 1.0f;

        VkPipelineViewportStateCreateInfo viewportState;
        memset( &viewportState, 0, sizeof( viewportState ) );
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkGraphicsPipelineCreateInfo pipelineInfo;
        memset( &pipelineInfo, 0, sizeof( pipelineInfo ) );
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = NULL; // optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_PipelineCache.Last()->hLayout;
        pipelineInfo.renderPass = g_pVKContext->GetRenderPass();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if ( vkCreateGraphicsPipelines( g_pVKContext->GetDevice(), m_hVulkanDataCache, 1, &pipelineInfo, NULL,
            &m_PipelineCache.Last()->hPipeline ) != VK_SUCCESS )
        {

        }
    }
    nCacheID = m_PipelineCache.Size();

    return nCacheID;
}

void VKShaderPipeline::SetShaderInputState( uint64_t nCacheID )
{
    m_nUsedPipeline = nCacheID;
}

void VKShaderPipeline::SaveVulkanPipelineCache( void )
{
    size_t nDataSize;
    void *pDataBuffer;

    vkGetPipelineCacheData( g_pVKContext->GetDevice(), m_hVulkanDataCache, &nDataSize, NULL );
    pDataBuffer = g_pVKContext->Alloc( nDataSize );
    vkGetPipelineCacheData( g_pVKContext->GetDevice(), m_hVulkanDataCache, &nDataSize, pDataBuffer );
}

void VKShaderPipeline::AllocateUniformBufferLayout( VKPipelineSet_t *pSet )
{
    VKShaderBuffer *pBuffer;
    VkDescriptorSetLayoutBinding *pLayoutBindings;
    VkDescriptorPoolSize *pPoolSizes;
    uint32_t i, j;

    pSet->descriptorSets.Reserve( NumUniforms );
    pLayoutBindings = (VkDescriptorSetLayoutBinding *)alloca( sizeof( *pLayoutBindings ) * NumUniforms );
    pPoolSizes = (VkDescriptorPoolSize *)alloca( sizeof( *pPoolSizes ) * NumUniforms );
    memset( pLayoutBindings, 0, sizeof( *pLayoutBindings ) * NumUniforms );
    memset( pPoolSizes, 0, sizeof( *pPoolSizes ) * NumUniforms );

    for ( i = 0; i < NumUniforms; i++ ) {
        pLayoutBindings[i].binding = szDefaultUniforms[i].nIndex;
        pLayoutBindings[i].descriptorCount = 1;

        switch ( szDefaultUniforms[i].nStage ) {
        case ST_VERTEX:
            pLayoutBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ST_FRAGMENT:
            pLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        };

        switch ( szDefaultUniforms[i].nType ) {
        case UniformType_Buffer:
            pLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        case UniformType_Sampler:
            pLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
        };
        
        pLayoutBindings[i].pImmutableSamplers = NULL;

        pPoolSizes[i].descriptorCount = e_MaxFPS.GetValue();
        pPoolSizes[i].type = pLayoutBindings[i].descriptorType;

        pBuffer = new VKShaderBuffer( pSet, i );

        pSet->descriptorSets.Push( pBuffer );
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    memset( &layoutInfo, 0, sizeof( layoutInfo ) );
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = NumUniforms;
    layoutInfo.pBindings = pLayoutBindings;

    if ( vkCreateDescriptorSetLayout( g_pVKContext->GetDevice(), &layoutInfo, NULL,
        &pSet->hDescriptorSetLayout ) != VK_SUCCESS )
    {

    }

    VkDescriptorPoolCreateInfo poolInfo;
    memset( &poolInfo, 0, sizeof( poolInfo ) );
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = NumUniforms;
    poolInfo.pPoolSizes = pPoolSizes;
    poolInfo.maxSets = e_MaxFPS.GetValue();

    if ( vkCreateDescriptorPool( g_pVKContext->GetDevice(), &poolInfo, NULL, &pSet->hDescriptorPool ) != VK_SUCCESS ) {

    }
}
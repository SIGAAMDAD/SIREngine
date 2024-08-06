#include "../RenderProgram.h"
#include "VKShaderPipeline.h"
#include "VKShader.h"
#include "VKShaderBuffer.h"
#include "VKContext.h"

static const VkFormat attribTypeVK[ NUMATTRIBTYPES ] = {
    VK_FORMAT_R32_SINT,
    VK_FORMAT_R32_SFLOAT,

    VK_FORMAT_R64_SINT,
    VK_FORMAT_R64_SFLOAT,

    VK_FORMAT_R32G32_SFLOAT,
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_FORMAT_R32G32B32A32_SFLOAT,

    VK_FORMAT_R32G32B32A32_SFLOAT,
    VK_FORMAT_R32G32B32A32_SFLOAT,

//    VK_FORMAT_R16G16B16A16_UINT,
//    VK_FORMAT_R16G16B16A16_SINT,
    
//    VK_FORMAT_R16_SINT,
//    VK_FORMAT_R16_UINT,
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
    VK_CALL( vkCreatePipelineCache( g_pVKContext->GetDevice(), &pipelineCacheInfo, g_pVKContext->GetAllocationCallbacks(), &m_hVulkanDataCache ) );

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
    VK_CALL( vkCreateSampler( g_pVKContext->GetDevice(), &samplerBilinearInfo, g_pVKContext->GetAllocationCallbacks(), &m_Samplers[0] ) );

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
    VK_CALL( vkCreateSampler( g_pVKContext->GetDevice(), &samplerNearestInfo, g_pVKContext->GetAllocationCallbacks(), &m_Samplers[1] ) );

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
    VK_CALL( vkCreateSampler( g_pVKContext->GetDevice(), &samplerLinearNearestInfo, g_pVKContext->GetAllocationCallbacks(), &m_Samplers[2] ) );

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
    VK_CALL( vkCreateSampler( g_pVKContext->GetDevice(), &samplerNearestLinearInfo, g_pVKContext->GetAllocationCallbacks(), &m_Samplers[3] ) );

    m_pVertexBuffer = dynamic_cast<VKBuffer *>( IRenderBuffer::Create( BUFFER_TYPE_VERTEX, BufferUsage_Stream, 64*1024 ) );
    m_pIndexBuffer = dynamic_cast<VKBuffer *>( IRenderBuffer::Create( BUFFER_TYPE_INDEX, BufferUsage_Stream, 64*1024 ) );
}

VKShaderPipeline::~VKShaderPipeline()
{
    ClearPipelineCache();

    delete m_pVertexBuffer;
    delete m_pIndexBuffer;
}

void VKShaderPipeline::ClearPipelineCache( void )
{
    uint32_t i;

    if ( m_hVulkanDataCache ) {
        vkDestroyPipelineCache( g_pVKContext->GetDevice(), m_hVulkanDataCache, g_pVKContext->GetAllocationCallbacks() );
    }

    for ( auto it : m_PipelineCache ) {
        if ( it->hLayout ) {
            vkDestroyPipelineLayout( g_pVKContext->GetDevice(), it->hLayout, g_pVKContext->GetAllocationCallbacks() );
        }
        if ( it->hPipeline ) {
            vkDestroyPipeline( g_pVKContext->GetDevice(), it->hPipeline, g_pVKContext->GetAllocationCallbacks() );
        }
        for ( i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ ) {
            if ( it->pDescriptorSetLayouts[i] ) {
                vkDestroyDescriptorSetLayout( g_pVKContext->GetDevice(), it->pDescriptorSetLayouts[i], g_pVKContext->GetAllocationCallbacks() );
            }
        }
        if ( it->hDescriptorPool ) {
            vkDestroyDescriptorPool( g_pVKContext->GetDevice(), it->hDescriptorPool, g_pVKContext->GetAllocationCallbacks() );
        }
        delete[] it->pDescriptorSetLayouts;
        delete[] it->pDescriptorSets;
    }

    for ( auto& it : m_Samplers ) {
        if ( it ) {
            vkDestroySampler( g_pVKContext->GetDevice(), it, g_pVKContext->GetAllocationCallbacks() );
        }
    }
    m_PipelineCache.clear();
}

uint64_t VKShaderPipeline::AddVertexAttribSet( const VertexInputDescription_t& vertexInput )
{
    uint64_t nCacheID;
    uint64_t nSize, nAttribCount, i, j;
    VKPipelineSet_t *pSet;

    nAttribCount = NumVertexAttribs( vertexInput );

    SIRENGINE_LOG( "Creating Vulkan Pipeline Input Set..." );

    nSize = 0;
    nSize += SIRENGINE_PAD( sizeof( *pSet ), sizeof( uintptr_t ) );
    nSize += SIRENGINE_PAD( sizeof( *pSet->pAttributes ) * nAttribCount, sizeof( uintptr_t ) );
    nSize += SIRENGINE_PAD( sizeof( *pSet->pBindings ) * nAttribCount, sizeof( uintptr_t ) );

    pSet = (VKPipelineSet_t *)g_pVKContext->Alloc( nSize, 16 );
    pSet->pAttributes = (VkVertexInputAttributeDescription *)( pSet + 1 );
    pSet->pBindings = (VkVertexInputBindingDescription *)( pSet->pAttributes + nAttribCount );

    pSet->nUniformCount = vertexInput.nUniformCount;

    m_PipelineCache.emplace_back( pSet );

    SIRENGINE_LOG( "Allocated VertexInput Attribute Layout..." );
    nAttribCount = 0;
    for ( i = 0; i < NumAttribs; i++ ) {
        const bool32 bEnabled = vertexInput.nEnabledVertexAttributes & SIRENGINE_BIT( i );

        if ( bEnabled ) {
            SIRENGINE_LOG( "- Pipeline VertexInput Attribute \"%s\" enabled", szDefaultVertexAttribs[i].pName );

            pSet->pBindings[ nAttribCount ].binding = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pBindings[ nAttribCount ].stride = szDefaultVertexAttribs[i].nStride;
            pSet->pBindings[ nAttribCount ].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            pSet->pAttributes[ nAttribCount ].binding = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pAttributes[ nAttribCount ].location = szDefaultVertexAttribs[i].nShaderBinding;
            pSet->pAttributes[ nAttribCount ].offset = szDefaultVertexAttribs[i].nOffset;
            pSet->pAttributes[ nAttribCount ].format = attribTypeVK[ szDefaultVertexAttribs[i].nType ];

            nAttribCount++;
        }
    }
    SIRENGINE_LOG( "Using %lu VertexInput Attributes.", nAttribCount );

    pSet->nAttribCount = nAttribCount;
    
    {
        AllocateUniformBufferLayout( pSet, vertexInput );

        VkPipelineLayoutCreateInfo createInfo;
        memset( &createInfo, 0, sizeof( createInfo ) );
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        createInfo.pushConstantRangeCount = 0;
        createInfo.pPushConstantRanges = NULL;
        createInfo.setLayoutCount = VK_MAX_FRAMES_IN_FLIGHT;
        createInfo.pSetLayouts = pSet->pDescriptorSetLayouts;

        VK_CALL( vkCreatePipelineLayout( g_pVKContext->GetDevice(), &createInfo, g_pVKContext->GetAllocationCallbacks(), &pSet->hLayout ) );
        SIRENGINE_LOG( "Created VkPipelineLayout" );
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
        fragmentStage.pName = "main";
        fragmentStage.module = dynamic_cast<VKShader *>( vertexInput.pShader->GetPixelShader() )->GetVKModule();

        VkPipelineShaderStageCreateInfo vertexStage;
        memset( &vertexStage, 0, sizeof( vertexStage ) );
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.pName = "main";
        vertexStage.module = dynamic_cast<VKShader *>( vertexInput.pShader->GetVertexShader() )->GetVKModule();

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexStage,
            fragmentStage
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly;
        memset( &inputAssembly, 0, sizeof( inputAssembly ) );
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
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
        pipelineInfo.layout = pSet->hLayout;
        pipelineInfo.renderPass = g_pVKContext->GetRenderPass();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        VK_CALL( vkCreateGraphicsPipelines( g_pVKContext->GetDevice(), NULL, 1, &pipelineInfo, g_pVKContext->GetAllocationCallbacks(),
            &pSet->hPipeline ) );
        SIRENGINE_LOG( "Finalized VkPipeline." );
    }
#if !defined(SIRENGINE_USE_VK_DESCRIPTOR_POOLS)
    {
        VkDescriptorUpdateTemplateEntryKHR *pUpdateEntries;

        pUpdateEntries = (VkDescriptorUpdateTemplateEntryKHR *)alloca( sizeof( *pUpdateEntries ) * NumUniforms );
        memset( pUpdateEntries, 0, sizeof( *pUpdateEntries ) * NumUniforms );

        for ( i = 0; i < NumUniforms; i++ ) {
            pUpdateEntries[i].dstBinding = szDefaultUniforms[i].nIndex;
        }

        VkDescriptorUpdateTemplateCreateInfoKHR templateInfo;
        memset( &templateInfo, 0, sizeof( templateInfo ) );
        templateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
        templateInfo.descriptorSetLayout = pSet->hDescriptorSetLayout;
        templateInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        templateInfo.pipelineLayout = pSet->hLayout;
        templateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
        templateInfo.set = 0;

        VK_CALL( fn_vkCreateDescriptorUpdateTemplateKHR( g_pVKContext->GetDevice(), &templateInfo, g_pVKContext->GetAllocationCallbacks(),
            &pSet->hUpdateTemplate ) );
        SIRENGINE_LOG( "Allocated VkDescriptorUpdateTemplate." );
    }
#endif
    nCacheID = m_PipelineCache.size();

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
    pDataBuffer = new char[ nDataSize ];
    vkGetPipelineCacheData( g_pVKContext->GetDevice(), m_hVulkanDataCache, &nDataSize, pDataBuffer );

    delete[] (char *)pDataBuffer;
}

static uint32_t GetDescriptorPoolUniformTypeCount( uint32_t nType, const VertexInputDescription_t& vertexInput )
{
    uint32_t count, i;

    count = 0;
    for ( i = 0; i < vertexInput.nUniformCount; i++ ) {
        if ( vertexInput.pUniformList[i].nType == nType ) {
            count++;
        }
    }

    return count;
}

void VKShaderPipeline::AllocateUniformBufferLayout( VKPipelineSet_t *pSet, const VertexInputDescription_t& vertexInput )
{
    VKShaderBuffer *pBuffer;
    VkDescriptorSetLayoutBinding *pLayoutBindings;
    VkDescriptorPoolSize szPoolSizes[ NumUniformTypes ];
    uint32_t i, j, nBindingCount;
    uint32_t nVertexBindingCount, nFragmentBindingCount;

    const VkDescriptorType szUniformDescriptorTypes[ NumUniformTypes ] = {
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    };

    SIRENGINE_LOG( "Allocating UniformBuffer Layout..." );

    memset( szPoolSizes, 0, sizeof( szPoolSizes ) );
    for ( i = 0; i < NumUniformTypes; i++ ) {
        szPoolSizes[i].descriptorCount = GetDescriptorPoolUniformTypeCount( i, vertexInput );
        szPoolSizes[i].type = szUniformDescriptorTypes[ i ];
    }

    VkDescriptorPoolCreateInfo poolInfo;
    memset( &poolInfo, 0, sizeof( poolInfo ) );
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = pSet->nUniformCount;
    poolInfo.poolSizeCount = NumUniformTypes;
    poolInfo.pPoolSizes = szPoolSizes;
    
    VK_CALL( vkCreateDescriptorPool( g_pVKContext->GetDevice(), &poolInfo, g_pVKContext->GetAllocationCallbacks(), &pSet->hDescriptorPool ) );
    
    pSet->pDescriptorSetLayouts = new VkDescriptorSetLayout[ sizeof( *pSet->pDescriptorSetLayouts ) * VK_MAX_FRAMES_IN_FLIGHT ];
    pSet->pDescriptorSets = new VkDescriptorSet[ sizeof( *pSet->pDescriptorSets ) * VK_MAX_FRAMES_IN_FLIGHT ];

    pLayoutBindings = (VkDescriptorSetLayoutBinding *)alloca( sizeof( *pLayoutBindings ) * pSet->nUniformCount );
    memset( pLayoutBindings, 0, sizeof( *pLayoutBindings ) * pSet->nUniformCount );

    for ( j = 0; j < pSet->nUniformCount; j++ ) {
        pLayoutBindings[j].binding = vertexInput.pUniformList[j].nIndex;
        pLayoutBindings[j].descriptorCount = vertexInput.pUniformList[j].nVariables;
        pLayoutBindings[j].descriptorType = szUniformDescriptorTypes[ vertexInput.pUniformList[j].nType ];
        pLayoutBindings[j].pImmutableSamplers = NULL;

        switch ( vertexInput.pUniformList[j].nStage ) {
        case ST_VERTEX:
            pLayoutBindings[j].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ST_FRAGMENT:
            pLayoutBindings[j].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case ST_GEOMETRY:
            pLayoutBindings[j].stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ST_TESSELATION:
            pLayoutBindings[j].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        };
    }

    VkDescriptorSetLayoutCreateInfo setLayoutInfo;
    memset( &setLayoutInfo, 0, sizeof( setLayoutInfo ) );
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = pSet->nUniformCount;
    setLayoutInfo.pBindings = pLayoutBindings;

    for ( i = 0; i < VK_MAX_FRAMES_IN_FLIGHT; i++ ) {
        VK_CALL( vkCreateDescriptorSetLayout( g_pVKContext->GetDevice(), &setLayoutInfo, g_pVKContext->GetAllocationCallbacks(),
            &pSet->pDescriptorSetLayouts[ i ] ) );
    }

    VkDescriptorSetAllocateInfo allocInfo;
    memset( &allocInfo, 0, sizeof( allocInfo ) );
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pSet->hDescriptorPool;
    allocInfo.descriptorSetCount = VK_MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = pSet->pDescriptorSetLayouts;

    VK_CALL( vkAllocateDescriptorSets( g_pVKContext->GetDevice(), &allocInfo, pSet->pDescriptorSets ) );
}

void VKShaderPipeline::ResetPipelineState( void )
{
}
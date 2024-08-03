#include "VKDescriptorManager.h"

VKDescriptorManager::VKDescriptorManager( void )
{
    m_UniformCache.reserve( NumUniforms );
}

VKDescriptorManager::~VKDescriptorManager()
{
}

DescriptorCacheEntry_t& VKDescriptorManager::AddUniformSet( const char *pName, const UniformInfo_t *pUniformInfo )
{
    VkDescriptorSetLayoutBinding *pBindings;
    uint64_t i;
    
    pBindings = (VkDescriptorSetLayoutBinding *)alloca( sizeof( *pBindings ) * pUniformInfo->nVariables );
    memset( pBindings, 0, sizeof( *pBindings ) * pUniformInfo->nVariables );
    for ( i = 0; i < pUniformInfo->nVariables; i++ ) {
        switch ( pUniformInfo[i].nStage ) {
        case ST_VERTEX:
            pBindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case ST_FRAGMENT:
            pBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case ST_GEOMETRY:
            pBindings[i].stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case ST_TESSELATION:
            pBindings[i].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        };

        pBindings[i].binding = pUniformInfo->nIndex;
        pBindings[i].descriptorCount = pUniformInfo->nVariables;
    }

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo;
    memset( &layoutCreateInfo, 0, sizeof( layoutCreateInfo ) );
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = pUniformInfo->nVariables;
    layoutCreateInfo.pBindings = pBindings;
}
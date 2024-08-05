#include "VKShader.h"
#include "VKContext.h"
#include <Engine/Core/FileSystem/MemoryFile.h>

VKShader::VKShader( const RenderShaderInit_t& shaderInfo )
{
    VkShaderStageFlagBits stage;
    const char *pSuffix;

    SIRENGINE_LOG( "Allocating VkShaderModule object \"%s\"", shaderInfo.pName );
    switch ( shaderInfo.nType ) {
    case ST_FRAGMENT:
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pSuffix = ".frag";
        break;
    case ST_VERTEX:
        stage = VK_SHADER_STAGE_VERTEX_BIT;
        pSuffix = ".vert";
        break;
    case ST_TESSELATION:
        stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        pSuffix = ".tess_control";
        break;
    case ST_GEOMETRY:
        stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        pSuffix = ".geom";
        break;
    default:
        SIRENGINE_ERROR( "Invalid Shader Type %i", shaderInfo.nType );
    };

    CMemoryFile file( SIRENGINE_TEMP_VSTRING( "Resources/Shaders/Vulkan/%s%s.spv", shaderInfo.pName, pSuffix ) );

    VkShaderModuleCreateInfo moduleInfo;
    memset( &moduleInfo, 0, sizeof( moduleInfo ) );
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = file.GetSize();
    moduleInfo.pCode = (const uint32_t *)file.GetBuffer();

    VK_CALL( vkCreateShaderModule( g_pVKContext->GetDevice(), &moduleInfo, NULL, &m_hModule ) );
}

VKShader::~VKShader()
{
    if ( m_hModule ) {
        vkDestroyShaderModule( g_pVKContext->GetDevice(), m_hModule, NULL );
    }
}
#include "VKShader.h"

VKShader::VKShader( const RenderShaderInit_t& shaderInfo )
{
    VkShaderStageFlagBits stage;
    const char *pSuffix;

    switch ( shaderInfo.nType ) {
    case ST_FRAGMENT:
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pSuffix = "FP";
        break;
    case ST_VERTEX:
        stage = VK_SHADER_STAGE_VERTEX_BIT;
        pSuffix = "VP";
        break;
    case ST_TESSELATION:
        stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        pSuffix = "TESS";
        break;
    case ST_GEOMETRY:
        stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        pSuffix = "GP";
        break;
    default:
        g_pApplication->Error( "" );
    };

    CMemoryFile file( SIRENGINE_TEMP_VSTRING( "%s%s", shaderInfo.pName, pSuffix ) );

    VkShaderModuleCreateInfo moduleInfo;
    memset( &moduleInfo, 0, sizeof( moduleInfo ) );
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = file.GetSize();
    moduleInfo.pCode = (const uint32_t *)file.GetBuffer();

    if ( vkCreateShaderModule( g_pVKContext->GetDevice(), &moduleInfo, NULL, &m_hModule ) != VK_SUCCESS ) {
        g_pApplication->Error( "" );
    }
}

VKShader::~VKShader()
{
    if ( m_hModule ) {
        vkDestroyShaderModule( g_pVKContext->GetDevice(), m_hModule, NULL );
    }
}
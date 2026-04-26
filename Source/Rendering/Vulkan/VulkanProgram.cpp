#include <Core/Pch.h>
#include "VulkanProgram.h"

using namespace SIREngine::Rendering::Vulkan;

VulkanProgram::VulkanProgram( VkDevice hDevice, const char *pszModuleName )
	: m_hDevice( hDevice )
{
	char szPath[ 256 ];

	snprintf( szPath, sizeof( szPath ) - 1, "Assets/Shaders/%s.frag.spv", pszModuleName );
	m_hFragmentModule = CreateShader( szPath );

	snprintf( szPath, sizeof( szPath ) - 1, "Assets/Shaders/%s.vert.spv", pszModuleName );
	m_hVertexModule = CreateShader( szPath );
}

VulkanProgram::~VulkanProgram()
{
	if ( m_hVertexModule != VK_NULL_HANDLE ) {
		vkDestroyShaderModule( m_hDevice, m_hVertexModule, NULL );
	}
	if ( m_hFragmentModule != VK_NULL_HANDLE ) {
		vkDestroyShaderModule( m_hDevice, m_hFragmentModule, NULL );
	}
}

VkShaderModule VulkanProgram::CreateShader( const char *pszFilePath )
{
	FILE *fp;
	fopen_s( &fp, pszFilePath, "rb" );
	
	fseek( fp, 0, SEEK_END );
	int length = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	char *pBuffer = (char *)alloca( length );
	fread( pBuffer, 1, length, fp );

	fclose( fp );

	VkShaderModuleCreateInfo createInfo;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = length;
	createInfo.pCode = (const uint32_t *)pBuffer;

	VkShaderModule hModule;
	VkResult result = vkCreateShaderModule( m_hDevice, &createInfo, NULL, &hModule );
	if ( result != VK_SUCCESS ) {
		printf( "Error creating VkShaderModule: %i\n", result );
	}
	
	return hModule;
}
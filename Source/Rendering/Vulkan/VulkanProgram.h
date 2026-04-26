#ifndef __VULKAN_PROGRAM_H__
#define __VULKAN_PROGRAM_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "Pch.h"
#include "../IRenderProgram.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanProgram : public IRenderProgram
	{
	public:
		VulkanProgram( VkDevice hDevice, const char *pszModuleName );
		virtual ~VulkanProgram() override;

		virtual void Compile( void ) override;
	private:
		VkShaderModule CreateShader( const char *pszFilePath );

		eastl::fixed_string<char, 72> m_szName;
		
		VkShaderModule m_arrModules[ (uint32_t)EShaderType::Count ];

		VkDevice m_hDevice;
	};
};

#endif
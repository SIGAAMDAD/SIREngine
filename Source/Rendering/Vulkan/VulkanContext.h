#ifndef __VULKAN_CONTEXT_H__
#define __VULKAN_CONTEXT_H__

#include "Pch.h"
#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "../DisplayConfig.h"
#include "../IRenderContext.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"

namespace SIREngine::Rendering::Vulkan
{
	class VulkanContext : public IRenderContext
	{
	public:
		VulkanContext( const SRenderConfig& config );
		virtual ~VulkanContext() override;

		virtual void Update( void ) override;
	private:
		VulkanInstance *m_pInstance;
	};
};

#endif
#ifndef __RENDER_CONFIG_H__
#define __RENDER_CONFIG_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include "DisplayConfig.h"

namespace SIREngine::Rendering
{
	enum class ERenderAPI : uint32_t
	{
		Vulkan,
		OpenGL,
		DirectX,

		Count
	};

	struct SRenderConfig
	{
		SDisplayConfig DisplayConfig;
		ERenderAPI eApi;
	};
};

#endif
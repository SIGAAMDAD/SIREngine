#ifndef __DISPLAY_CONFIG_H__
#define __DISPLAY_CONFIG_H__

#include <Core/Compiler.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Core/Pch.h>

namespace SIREngine::Rendering
{
	struct SDisplayConfig
	{
		const char *pszWindowName;
		uint32_t nWidth;
		uint32_t nHeight;
		float fAspectRatio;
	};
};

#endif
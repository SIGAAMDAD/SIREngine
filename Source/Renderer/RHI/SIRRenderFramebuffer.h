/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRRENDERFRAMEBUFFER_H__
#define __SIRENGINE_SIRRENDERFRAMEBUFFER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Logical render-target attachment collection. It does not allocate images.
// Vulkan may use dynamic rendering, D3D12 RTV/DSV and OpenGL a cached FBO.
// The descriptor borrows attachment arrays for the duration of command record.
class SIRRenderFramebuffer final
{
public:
	SIRRenderFramebuffer( void ) noexcept = default;
	explicit SIRRenderFramebuffer( const sirRenderFramebufferDesc_t& desc ) noexcept : m_Desc( desc )
	{
	}

	const sirRenderFramebufferDesc_t& getDesc( void ) const noexcept
	{
		return m_Desc;
	}
	bool isValid( void ) const noexcept
	{
		return m_Desc.nWidth != 0 && m_Desc.nHeight != 0 && m_Desc.nLayers != 0 && m_Desc.nSampleCount != 0 && m_Desc.nColorCount <= 8 &&
		       ( m_Desc.nColorCount == 0 || m_Desc.pColors != nullptr ) &&
		       ( m_Desc.nColorCount != 0 || m_Desc.depthStencil.hView.nGeneration != 0 );
	}

private:
	sirRenderFramebufferDesc_t m_Desc = {};
};

#endif // __SIRENGINE_SIRRENDERFRAMEBUFFER_H__
